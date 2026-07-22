import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM navigator/matchMedia', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('navigator exposes userAgent/language/onLine', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        hasUserAgent: typeof navigator.userAgent === 'string' && navigator.userAgent.length > 0,
        language: navigator.language,
        onLine: navigator.onLine,
      });
    `);
    expect(JSON.parse(result)).toEqual({ hasUserAgent: true, language: 'en-US', onLine: true });
  });

  it('matchMedia returns a MediaQueryList-like stub that never matches', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const mql = matchMedia('(min-width: 600px)');
      let changeFired = false;
      mql.addEventListener('change', () => { changeFired = true; });
      JSON.stringify({ media: mql.media, matches: mql.matches, changeFired });
    `);
    expect(JSON.parse(result)).toEqual({ media: '(min-width: 600px)', matches: false, changeFired: false });
  });
});
