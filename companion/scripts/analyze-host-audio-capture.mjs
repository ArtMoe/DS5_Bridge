import { readFileSync, statSync } from 'node:fs';
import path from 'node:path';

const FRAME_RECORD_PREFIX_BYTES = 2;
const HOST_AUDIO_FRAME_BYTES = 264;
const HAPTIC_BYTES = 64;
const HAPTIC_BUCKETS = 32;
const TARGET_SAMPLE_RATE = 48000;
const PICO_INPUT_BLOCK_FRAMES = 512;

const capturePath = process.argv[2];
if (!capturePath) {
  console.error('usage: node scripts/analyze-host-audio-capture.mjs <capture.bin>');
  process.exit(1);
}

const fullPath = path.resolve(capturePath);
const data = readFileSync(fullPath);
const fileSize = statSync(fullPath).size;
const summary = analyzeCapture(data);
console.log(JSON.stringify({
  path: fullPath,
  bytes: fileSize,
  ...summary
}, null, 2));

function analyzeCapture(data) {
  let offset = 0;
  let frames = 0;
  let malformedRecords = 0;
  let trailingBytes = 0;
  let activeFrames = 0;
  let silentRun = 0;
  let maxSilentRun = 0;
  let opusNonZeroBytes = 0;
  const left = createChannelStats();
  const right = createChannelStats();

  while (offset + FRAME_RECORD_PREFIX_BYTES <= data.length) {
    const frameLength = data.readUInt16LE(offset);
    offset += FRAME_RECORD_PREFIX_BYTES;
    if (frameLength !== HOST_AUDIO_FRAME_BYTES) {
      malformedRecords++;
      if (offset + frameLength > data.length) {
        trailingBytes = data.length - (offset - FRAME_RECORD_PREFIX_BYTES);
        break;
      }
      offset += frameLength;
      continue;
    }
    if (offset + frameLength > data.length) {
      trailingBytes = data.length - (offset - FRAME_RECORD_PREFIX_BYTES);
      break;
    }

    const frame = data.subarray(offset, offset + frameLength);
    offset += frameLength;
    frames++;

    let framePeak = 0;
    for (let bucket = 0; bucket < HAPTIC_BUCKETS; bucket++) {
      const leftValue = signedByte(frame[bucket * 2]);
      const rightValue = signedByte(frame[bucket * 2 + 1]);
      addSample(left, leftValue);
      addSample(right, rightValue);
      framePeak = Math.max(framePeak, Math.abs(leftValue), Math.abs(rightValue));
    }

    if (framePeak > 0) {
      activeFrames++;
      silentRun = 0;
    } else {
      silentRun++;
      maxSilentRun = Math.max(maxSilentRun, silentRun);
    }

    for (let index = HAPTIC_BYTES; index < frame.length; index++) {
      if (frame[index] !== 0) {
        opusNonZeroBytes++;
      }
    }
  }

  if (offset < data.length) {
    trailingBytes = data.length - offset;
  }

  return {
    frames,
    durationSeconds: round(frames * PICO_INPUT_BLOCK_FRAMES / TARGET_SAMPLE_RATE, 3),
    malformedRecords,
    trailingBytes,
    hapticActiveFrames: activeFrames,
    hapticActivePercent: frames === 0 ? 0 : round(activeFrames * 100 / frames, 2),
    maxSilentRunFrames: maxSilentRun,
    maxSilentRunMs: round(maxSilentRun * PICO_INPUT_BLOCK_FRAMES * 1000 / TARGET_SAMPLE_RATE, 2),
    hapticLeft: finishChannelStats(left),
    hapticRight: finishChannelStats(right),
    opusNonZeroBytes
  };
}

function createChannelStats() {
  return {
    samples: 0,
    nonZeroSamples: 0,
    peak: 0,
    absoluteSum: 0,
    squareSum: 0
  };
}

function addSample(stats, value) {
  const abs = Math.abs(value);
  stats.samples++;
  if (value !== 0) {
    stats.nonZeroSamples++;
  }
  stats.peak = Math.max(stats.peak, abs);
  stats.absoluteSum += abs;
  stats.squareSum += value * value;
}

function finishChannelStats(stats) {
  return {
    peak: stats.peak,
    peakPercent: round(stats.peak * 100 / 127, 2),
    rms: stats.samples === 0 ? 0 : round(Math.sqrt(stats.squareSum / stats.samples), 3),
    rmsPercent: stats.samples === 0 ? 0 : round(Math.sqrt(stats.squareSum / stats.samples) * 100 / 127, 2),
    meanAbs: stats.samples === 0 ? 0 : round(stats.absoluteSum / stats.samples, 3),
    nonZeroSamples: stats.nonZeroSamples,
    nonZeroPercent: stats.samples === 0 ? 0 : round(stats.nonZeroSamples * 100 / stats.samples, 2)
  };
}

function signedByte(value) {
  return value > 127 ? value - 256 : value;
}

function round(value, decimals) {
  const scale = 10 ** decimals;
  return Math.round(value * scale) / scale;
}
