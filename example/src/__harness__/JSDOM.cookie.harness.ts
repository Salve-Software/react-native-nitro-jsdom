import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM document.cookie', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('defaults to an empty string', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate('document.cookie');
    expect(result).toBe('');
  });

  it('setting a single cookie makes it readable back', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    await dom.evaluate(`document.cookie = 'foo=bar'`);
    const result = await dom.evaluate('document.cookie');
    expect(result).toBe('foo=bar');
  });

  it('setting multiple cookies joins them with "; ", in insertion order', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    await dom.evaluate(`
      document.cookie = 'a=1';
      document.cookie = 'b=2';
    `);
    const result = await dom.evaluate('document.cookie');
    expect(result).toBe('a=1; b=2');
  });

  it('re-setting an existing cookie name updates its value without duplicating it', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    await dom.evaluate(`
      document.cookie = 'a=1';
      document.cookie = 'b=2';
      document.cookie = 'a=updated';
    `);
    const result = await dom.evaluate('document.cookie');
    expect(result).toBe('a=updated; b=2');
  });

  it('attributes after the first ";" (expires/path/domain/etc) are discarded, not enforced', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    await dom.evaluate(`document.cookie = 'a=1; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=/'`);
    const result = await dom.evaluate('document.cookie');
    expect(result).toBe('a=1');
  });
});
