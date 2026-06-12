import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

const preloadSource = readFileSync(new URL('../preload.ts', import.meta.url), 'utf8');
const mainSource = readFileSync(new URL('./main.ts', import.meta.url), 'utf8');

function matches(source: string, pattern: RegExp): string[] {
  return [...source.matchAll(pattern)].map((match) => match[1] ?? '');
}

function uniqueSorted(values: string[]): string[] {
  return [...new Set(values)].sort();
}

function duplicateValues(values: string[]): string[] {
  const seen = new Set<string>();
  const duplicates = new Set<string>();
  for (const value of values) {
    if (seen.has(value)) {
      duplicates.add(value);
    }
    seen.add(value);
  }
  return [...duplicates].sort();
}

describe('IPC contract', () => {
  it('keeps every preload invoke channel backed by exactly one main handler', () => {
    const preloadChannels = matches(preloadSource, /ipcRenderer\.invoke\('([^']+)'/g);
    const mainChannels = matches(mainSource, /ipcMain\.handle\('([^']+)'/g);

    expect(duplicateValues(preloadChannels)).toEqual([]);
    expect(duplicateValues(mainChannels)).toEqual([]);
    expect(uniqueSorted(preloadChannels)).toEqual(uniqueSorted(mainChannels));
  });

  it('returns unsubscribe functions for renderer event subscriptions', () => {
    expect(preloadSource).toContain("ipcRenderer.on('window:maximizedChanged', listener)");
    expect(preloadSource).toContain("ipcRenderer.removeListener('window:maximizedChanged', listener)");
    expect(preloadSource).toContain("ipcRenderer.on('bridge:snapshot', listener)");
    expect(preloadSource).toContain("ipcRenderer.removeListener('bridge:snapshot', listener)");
    expect(mainSource).toContain("sendToMainWindow('bridge:snapshot', snapshot)");
    expect(mainSource).toContain("mainWindow.webContents.send('window:maximizedChanged', mainWindow.isMaximized())");
  });
});
