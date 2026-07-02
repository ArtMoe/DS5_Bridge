import fs from 'node:fs/promises';
import path from 'node:path';
import type { PicoFirmwareActionResult } from '../shared/types';

export const PICO_UNIVERSAL_FLASH_NUKE_FILE = 'pico-universal-flash-nuke.uf2';

const PICO_BOOTLOADER_WAIT_MS = 15000;
const PICO_BOOTLOADER_POLL_MS = 250;

export interface PicoBootloaderDrive {
  root: string;
  info: string;
}

export interface PicoFirmwareUpdateOptions {
  enterBootloader: () => Promise<void>;
  driveRoots?: string[];
  nukeUf2Path?: string;
}

function windowsDriveRoots(): string[] {
  return Array.from({ length: 26 }, (_, index) => `${String.fromCharCode(65 + index)}:\\`);
}

function defaultDriveRoots(): string[] {
  return process.platform === 'win32' ? windowsDriveRoots() : [];
}

function normalizeDriveRoot(root: string): string {
  return path.resolve(root);
}

function isPicoBootloaderInfo(text: string): boolean {
  return /UF2 Bootloader/i.test(text) && /(RPI-RP2|RP2040|RP2350|Pico|Board-ID)/i.test(text);
}

async function readPicoBootloaderInfo(root: string): Promise<string | null> {
  try {
    const info = await fs.readFile(path.join(root, 'INFO_UF2.TXT'), 'utf8');
    return isPicoBootloaderInfo(info) ? info : null;
  } catch {
    return null;
  }
}

export async function findPicoBootloaderDrive(driveRoots = defaultDriveRoots()): Promise<PicoBootloaderDrive | null> {
  for (const root of driveRoots) {
    const info = await readPicoBootloaderInfo(root);
    if (info) {
      return { root: normalizeDriveRoot(root), info };
    }
  }
  return null;
}

async function wait(ms: number): Promise<void> {
  await new Promise((resolve) => {
    setTimeout(resolve, ms);
  });
}

export async function waitForPicoBootloaderDrive(
  driveRoots = defaultDriveRoots(),
  timeoutMs = PICO_BOOTLOADER_WAIT_MS
): Promise<PicoBootloaderDrive | null> {
  const deadline = Date.now() + timeoutMs;
  do {
    const drive = await findPicoBootloaderDrive(driveRoots);
    if (drive) {
      return drive;
    }
    await wait(PICO_BOOTLOADER_POLL_MS);
  } while (Date.now() < deadline);
  return null;
}

async function ensurePicoBootloaderDrive(options: PicoFirmwareUpdateOptions): Promise<PicoBootloaderDrive> {
  const mountedDrive = await findPicoBootloaderDrive(options.driveRoots);
  if (mountedDrive) {
    return mountedDrive;
  }

  await options.enterBootloader();
  const drive = await waitForPicoBootloaderDrive(options.driveRoots);
  if (!drive) {
    throw new Error('Pico UF2 bootloader drive was not found. Hold BOOTSEL while plugging in the Pico, then try again.');
  }
  return drive;
}

async function assertUf2File(sourcePath: string): Promise<void> {
  if (path.extname(sourcePath).toLowerCase() !== '.uf2') {
    throw new Error('Choose a .uf2 firmware file.');
  }
  const stat = await fs.stat(sourcePath);
  if (!stat.isFile()) {
    throw new Error('Choose a .uf2 firmware file.');
  }
}

async function copyUf2ToDrive(sourcePath: string, drive: PicoBootloaderDrive): Promise<string> {
  await assertUf2File(sourcePath);
  const targetPath = path.join(drive.root, path.basename(sourcePath));
  await fs.copyFile(sourcePath, targetPath);
  return targetPath;
}

export async function mountPicoBootloaderDrive(
  options: PicoFirmwareUpdateOptions
): Promise<PicoFirmwareActionResult> {
  const alreadyMounted = await findPicoBootloaderDrive(options.driveRoots);
  if (alreadyMounted) {
    return {
      ok: true,
      action: 'mount',
      driveRoot: alreadyMounted.root,
      message: `Pico bootloader is already mounted at ${alreadyMounted.root}.`
    };
  }

  await options.enterBootloader();
  const drive = await waitForPicoBootloaderDrive(options.driveRoots);
  if (!drive) {
    throw new Error('Pico UF2 bootloader drive was not found. Hold BOOTSEL while plugging in the Pico, then try again.');
  }
  return {
    ok: true,
    action: 'mount',
    driveRoot: drive.root,
    message: `Pico bootloader mounted at ${drive.root}.`
  };
}

export async function flashPicoFirmwareUf2(
  sourcePath: string,
  options: PicoFirmwareUpdateOptions
): Promise<PicoFirmwareActionResult> {
  const drive = await ensurePicoBootloaderDrive(options);
  const targetPath = await copyUf2ToDrive(sourcePath, drive);
  return {
    ok: true,
    action: 'flash',
    driveRoot: drive.root,
    sourcePath,
    targetPath,
    message: `Copied ${path.basename(sourcePath)} to ${drive.root}. The Pico should reboot automatically.`
  };
}

export async function nukePicoFlash(
  options: PicoFirmwareUpdateOptions
): Promise<PicoFirmwareActionResult> {
  if (!options.nukeUf2Path) {
    throw new Error('Pico flash nuke UF2 is missing. Run tools\\build-pico-universal-flash-nuke.ps1 from the repository root.');
  }
  const sourcePath = options.nukeUf2Path;
  const drive = await ensurePicoBootloaderDrive(options);
  const targetPath = await copyUf2ToDrive(sourcePath, drive);
  return {
    ok: true,
    action: 'nuke',
    driveRoot: drive.root,
    sourcePath,
    targetPath,
    message: `Copied ${PICO_UNIVERSAL_FLASH_NUKE_FILE} to ${drive.root}. Wait for the Pico to remount, then flash firmware.`
  };
}
