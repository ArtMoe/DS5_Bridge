import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';
import {
  PICO_UNIVERSAL_FLASH_NUKE_FILE,
  findPicoBootloaderDrive,
  flashPicoFirmwareUf2,
  mountPicoBootloaderDrive,
  nukePicoFlash
} from './pico-firmware-updater';

const tempRoots: string[] = [];

function tempDir(): string {
  const dir = mkdtempSync(path.join(os.tmpdir(), 'ds5-pico-'));
  tempRoots.push(dir);
  return dir;
}

function writeBootloaderInfo(root: string): void {
  writeFileSync(path.join(root, 'INFO_UF2.TXT'), 'UF2 Bootloader\nBoard-ID: RP2350\n', 'utf8');
}

afterEach(() => {
  while (tempRoots.length > 0) {
    const root = tempRoots.pop();
    if (root) {
      rmSync(root, { recursive: true, force: true });
    }
  }
});

describe('Pico firmware updater', () => {
  it('finds a mounted Pico UF2 bootloader drive by INFO_UF2.TXT', async () => {
    const emptyDrive = tempDir();
    const picoDrive = tempDir();
    writeBootloaderInfo(picoDrive);

    const drive = await findPicoBootloaderDrive([emptyDrive, picoDrive]);

    expect(drive?.root).toBe(path.resolve(picoDrive));
    expect(drive?.info).toContain('RP2350');
  });

  it('asks firmware to enter BOOTSEL mode before waiting for the drive', async () => {
    const picoDrive = tempDir();
    let enterBootloaderCalled = false;

    const result = await mountPicoBootloaderDrive({
      driveRoots: [picoDrive],
      enterBootloader: async () => {
        enterBootloaderCalled = true;
        writeBootloaderInfo(picoDrive);
      }
    });

    expect(enterBootloaderCalled).toBe(true);
    expect(result.ok).toBe(true);
    expect(result.driveRoot).toBe(path.resolve(picoDrive));
  });

  it('copies a selected UF2 to the mounted Pico drive', async () => {
    const picoDrive = tempDir();
    const firmwareDir = tempDir();
    const sourcePath = path.join(firmwareDir, 'ds5-bridge.uf2');
    writeBootloaderInfo(picoDrive);
    writeFileSync(sourcePath, 'firmware');

    const result = await flashPicoFirmwareUf2(sourcePath, {
      driveRoots: [picoDrive],
      enterBootloader: async () => undefined
    });

    expect(result.ok).toBe(true);
    expect(result.targetPath).toBe(path.join(path.resolve(picoDrive), 'ds5-bridge.uf2'));
    expect(readFileSync(result.targetPath!, 'utf8')).toBe('firmware');
  });

  it('copies the bundled Pico Universal Flash Nuke when nuking', async () => {
    const picoDrive = tempDir();
    const nukeDir = tempDir();
    const nukePath = path.join(nukeDir, PICO_UNIVERSAL_FLASH_NUKE_FILE);
    writeBootloaderInfo(picoDrive);
    writeFileSync(nukePath, 'nuke');

    const result = await nukePicoFlash({
      driveRoots: [picoDrive],
      enterBootloader: async () => undefined,
      nukeUf2Path: nukePath
    });

    expect(result.ok).toBe(true);
    expect(result.action).toBe('nuke');
    expect(readFileSync(result.targetPath!, 'utf8')).toBe('nuke');
  });

  it('fails nuke without a locally built UF2 path', async () => {
    await expect(nukePicoFlash({
      driveRoots: [tempDir()],
      enterBootloader: async () => undefined
    })).rejects.toThrow('Pico flash nuke UF2 is missing');
  });
});
