import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

const appSource = readFileSync(path.join(path.dirname(fileURLToPath(import.meta.url)), 'App.tsx'), 'utf8');

function extractFunction(name: string): string {
  const start = appSource.indexOf(`function ${name}`);
  expect(start).toBeGreaterThanOrEqual(0);
  const nextFunction = appSource.indexOf('\n  function ', start + 1);
  return appSource.slice(start, nextFunction === -1 ? undefined : nextFunction);
}

describe('renderer behavior guards', () => {
  it('requires explicit confirmation before disabling Host Encoding', () => {
    const toggleFunction = extractFunction('toggleHostEncodedAudioEnabled');
    const confirmFunction = extractFunction('confirmDisableHostEncoding');

    expect(toggleFunction).toContain('setHostEncodingDisableConfirmVisible(true)');
    expect(toggleFunction).not.toContain('setHostEncodedAudioEnabled(false)');
    expect(confirmFunction).toContain('setHostEncodedAudioEnabled(false)');
    expect(appSource).toContain('Disable Host Encoding?');
    expect(appSource).toContain('Turning it off may cause audio stuttering.');
  });

  it('requires explicit confirmation and a disconnected controller before emergency device repair', () => {
    const openFunction = extractFunction('openDeviceCleanupConfirm');
    const runFunction = extractFunction('runWindowsDeviceCleanup');

    expect(appSource).toContain('IconTool');
    expect(openFunction).toContain('setDeviceCleanupConfirmVisible(true)');
    expect(runFunction).toContain('controllerConnected');
    expect(runFunction).toContain('repairWindowsDeviceCache');
    expect(appSource).toContain('Emergency Device Repair');
    expect(appSource).toContain('Only run this if you are running into persistent odd controller');
    expect(appSource).toContain('Disconnect the controller from the bridge');
    expect(appSource).toContain('Controller identity based profiles');
    expect(appSource).toContain('paired directly to Windows over Bluetooth may need to be paired again');
  });

  it('treats host encoded audio as haptic-test audio activity', () => {
    expect(appSource).toContain('const feedbackAudioActive = audioStreamActive');
    expect(appSource).toContain('|| hostAudioActive');
    expect(appSource).toContain('Boolean(hostAudioEnabled && hostAudioStatus?.streamActive)');
    expect(appSource).toContain('|| feedbackAudioActive');
  });

  it('keeps haptic test and cooldown labels ahead of audio-busy labels', () => {
    const start = appSource.indexOf('<button className="primary-action" type="button" disabled={activeFeedbackTestUnavailable}');
    expect(start).toBeGreaterThanOrEqual(0);
    const end = appSource.indexOf('</button>', start);
    const buttonSource = appSource.slice(start, end);

    expect(buttonSource).toContain('feedbackAudioLabel');
    expect(buttonSource.indexOf('testLocked')).toBeLessThan(buttonSource.indexOf('feedbackAudioActive'));
    expect(buttonSource.indexOf('snapshot.status?.testHapticsCooldown')).toBeLessThan(
      buttonSource.lastIndexOf('feedbackAudioActive')
    );
  });
});
