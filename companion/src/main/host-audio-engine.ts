import { spawn, type ChildProcess, type ChildProcessWithoutNullStreams } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { EventEmitter } from 'node:events';

export type HostAudioFramePayload = {
  frame: number[];
  sequence: number;
  encodedBytes: number;
};

const FRAME_RECORD_PREFIX_BYTES = 2;
const HOST_AUDIO_FRAME_BYTES = 264;
const HELPER_RELATIVE_PATH = path.join('native', 'HostAudioHelper', 'HostAudioHelper.exe');
const DEV_HELPER_RELATIVE_PATH = path.join(
  'native',
  'HostAudioHelper',
  'bin',
  'publish',
  'win-x64',
  'HostAudioHelper.exe'
);

export class HostAudioEngine extends EventEmitter {
  private process: ChildProcessWithoutNullStreams | null = null;
  private starting: Promise<void> | null = null;
  private stdoutBuffer = Buffer.alloc(0);
  private sequence = 0;
  private activeHidPath: string | null = null;
  private activeSpeakerVolumePercent = 100;

  async start(hidPath: string | null, speakerVolumePercent = 100): Promise<void> {
    const nextSpeakerVolumePercent = normalizeSpeakerVolumePercent(speakerVolumePercent);
    if (this.process) {
      if (this.activeHidPath !== hidPath) {
        await this.stop();
      } else {
        this.setSpeakerVolumePercent(nextSpeakerVolumePercent);
        return;
      }
    }
    if (this.process) {
      return;
    }
    if (this.starting) {
      await this.starting;
      if (this.process && this.activeHidPath === hidPath) {
        this.setSpeakerVolumePercent(nextSpeakerVolumePercent);
        return;
      }
      return this.start(hidPath, nextSpeakerVolumePercent);
    }

    this.starting = this.startInternal(hidPath, nextSpeakerVolumePercent).finally(() => {
      this.starting = null;
    });
    return this.starting;
  }

  setSpeakerVolumePercent(percent: number): void {
    const nextSpeakerVolumePercent = normalizeSpeakerVolumePercent(percent);
    this.activeSpeakerVolumePercent = nextSpeakerVolumePercent;
    const helper = this.process;
    if (!helper || helper.stdin.destroyed || !helper.stdin.writable) {
      return;
    }
    helper.stdin.write(`speaker-volume ${nextSpeakerVolumePercent}\n`, (error) => {
      if (error) {
        this.emit('error', error);
      }
    });
  }

  async stop(): Promise<void> {
    const helper = this.process;
    this.process = null;
    this.activeHidPath = null;
    this.activeSpeakerVolumePercent = 100;
    this.stdoutBuffer = Buffer.alloc(0);
    this.sequence = 0;
    if (!helper) {
      return;
    }

    await new Promise<void>((resolve) => {
      const timeout = setTimeout(() => {
        if (!helper.killed) {
          helper.kill('SIGKILL');
        }
        resolve();
      }, 500);

      helper.once('exit', () => {
        clearTimeout(timeout);
        resolve();
      });

      helper.stdin?.end();
      helper.kill();
    });
  }

  isActive(): boolean {
    return this.process !== null;
  }

  private async startInternal(hidPath: string | null, speakerVolumePercent: number): Promise<void> {
    const helperPath = resolveHelperPath();
    const args = ['--device-name', 'DS5 Bridge', '--speaker-volume', `${speakerVolumePercent}`];
    if (hidPath) {
      args.push('--hid-path', hidPath);
    }
    const helper = spawn(helperPath, args, {
      windowsHide: true,
      stdio: ['pipe', 'pipe', 'pipe']
    });

    this.process = helper;
    this.activeHidPath = hidPath;
    this.activeSpeakerVolumePercent = speakerVolumePercent;
    helper.stdout.on('data', (chunk: Buffer) => this.processStdout(chunk));
    helper.stderr.on('data', (chunk: Buffer) => {
      this.emit('status', chunk.toString('utf8').trim());
    });
    helper.on('error', (error) => this.emit('error', error));
    helper.on('exit', (code, signal) => {
      if (this.process === helper) {
        this.process = null;
        this.activeHidPath = null;
        this.activeSpeakerVolumePercent = 100;
        this.stdoutBuffer = Buffer.alloc(0);
        this.emit('status', `host audio helper exited (${signal ?? code ?? 'unknown'})`);
      }
    });
  }

  private processStdout(chunk: Buffer): void {
    this.stdoutBuffer = Buffer.concat([this.stdoutBuffer, chunk]);
    while (this.stdoutBuffer.length >= FRAME_RECORD_PREFIX_BYTES) {
      const frameLength = this.stdoutBuffer.readUInt16LE(0);
      const recordLength = FRAME_RECORD_PREFIX_BYTES + frameLength;
      if (this.stdoutBuffer.length < recordLength) {
        return;
      }

      const frame = this.stdoutBuffer.subarray(FRAME_RECORD_PREFIX_BYTES, recordLength);
      this.stdoutBuffer = this.stdoutBuffer.subarray(recordLength);
      if (frame.length !== HOST_AUDIO_FRAME_BYTES) {
        this.emit('error', new Error(`Unexpected host audio frame length ${frame.length}`));
        continue;
      }

      this.emit('frame', {
        frame: [...frame],
        sequence: this.sequence,
        encodedBytes: Math.max(0, frame.length - 64)
      } satisfies HostAudioFramePayload);
      this.sequence = (this.sequence + 1) & 0xffff;
    }
  }
}

function normalizeSpeakerVolumePercent(percent: number): number {
  return Math.max(0, Math.min(100, Math.round(percent)));
}

export class MicKeepaliveEngine extends EventEmitter {
  private process: ChildProcess | null = null;
  private starting: Promise<void> | null = null;

  async start(): Promise<void> {
    if (this.process) {
      return;
    }
    if (this.starting) {
      return this.starting;
    }

    this.starting = this.startInternal().finally(() => {
      this.starting = null;
    });
    return this.starting;
  }

  async stop(): Promise<void> {
    const helper = this.process;
    this.process = null;
    if (!helper) {
      return;
    }

    await new Promise<void>((resolve) => {
      const timeout = setTimeout(() => {
        if (!helper.killed) {
          helper.kill('SIGKILL');
        }
        resolve();
      }, 500);

      helper.once('exit', () => {
        clearTimeout(timeout);
        resolve();
      });

      helper.stdin?.end();
      helper.kill();
    });
  }

  isActive(): boolean {
    return this.process !== null;
  }

  private async startInternal(): Promise<void> {
    const helperPath = resolveHelperPath();
    const helper = spawn(helperPath, ['--mic-keepalive-only', '--mic-device-name', 'DS5 Bridge'], {
      windowsHide: true,
      stdio: ['pipe', 'ignore', 'pipe']
    });

    this.process = helper;
    helper.stderr.on('data', (chunk: Buffer) => {
      this.emit('status', chunk.toString('utf8').trim());
    });
    helper.on('error', (error) => this.emit('error', error));
    helper.on('exit', (code, signal) => {
      if (this.process === helper) {
        this.process = null;
        this.emit('status', `mic keepalive helper exited (${signal ?? code ?? 'unknown'})`);
      }
    });
  }
}

function resolveHelperPath(): string {
  const candidates = [
    process.resourcesPath ? path.join(process.resourcesPath, HELPER_RELATIVE_PATH) : null,
    path.resolve(process.cwd(), DEV_HELPER_RELATIVE_PATH),
    path.resolve(__dirname, '..', '..', '..', DEV_HELPER_RELATIVE_PATH)
  ].filter((candidate): candidate is string => Boolean(candidate));

  const helperPath = candidates.find((candidate) => existsSync(candidate));
  if (!helperPath) {
    throw new Error(`Host audio helper is missing. Run npm run build:host-audio from companion/.`);
  }
  return helperPath;
}
