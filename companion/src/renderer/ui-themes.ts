import kofiBadgeBeigeUrl from '../../../assets/brand/support_me_on_kofi_badge_beige.png';
import kofiBadgeBlueUrl from '../../../assets/brand/support_me_on_kofi_badge_blue.png';
import kofiBadgeDarkUrl from '../../../assets/brand/support_me_on_kofi_badge_dark.png';
import kofiBadgeRedUrl from '../../../assets/brand/support_me_on_kofi_badge_red.png';
import type { UiThemePreset } from '../shared/types';

export const DEFAULT_UI_THEME_PRESET: UiThemePreset = 'dark';

export const UI_THEME_OPTIONS: Array<[string, UiThemePreset]> = [
  ['Light', 'light'],
  ['Dark', 'dark'],
  ['Bubble Gum', 'bubble-gum'],
  ['Pomegranate', 'pomegranate'],
  ['Kiwi', 'kiwi']
];

export const UI_THEME_SWATCHES: Record<UiThemePreset, readonly string[]> = {
  light: ['#f4f7fb', '#dce7f5', '#1f6bd6'],
  dark: ['#050913', '#07111c', '#287cff'],
  'bubble-gum': ['#f69bd7', '#d44799', '#dc78c4'],
  pomegranate: ['#170d1d', '#d01868', '#8cff00'],
  kiwi: ['#030705', '#0d1f13', '#9cff2f']
};

export const UI_THEME_KOFI_BADGES: Record<UiThemePreset, string> = {
  light: kofiBadgeBeigeUrl,
  dark: kofiBadgeDarkUrl,
  'bubble-gum': kofiBadgeBlueUrl,
  pomegranate: kofiBadgeRedUrl,
  kiwi: kofiBadgeDarkUrl
};
