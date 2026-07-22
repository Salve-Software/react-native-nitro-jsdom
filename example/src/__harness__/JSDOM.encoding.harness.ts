import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM atob/btoa', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('btoa encodes a Latin1 string to base64', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`btoa('hello')`);
    expect(result).toBe('aGVsbG8=');
  });

  it('atob decodes a base64 string back to the original', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`atob('aGVsbG8=')`);
    expect(result).toBe('hello');
  });

  it('btoa/atob round-trip arbitrary bytes including padding edge cases', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify(['a', 'ab', 'abc', 'abcd'].map((s) => atob(btoa(s))));
    `);
    expect(JSON.parse(result)).toEqual(['a', 'ab', 'abc', 'abcd']);
  });

  it('btoa throws InvalidCharacterError DOMException for non-Latin1 input', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let caught;
      try { btoa('日本語'); } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'InvalidCharacterError', isDOMException: true });
  });

  it('atob throws InvalidCharacterError DOMException for malformed base64', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let caught;
      try { atob('not-valid-base64!!'); } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'InvalidCharacterError', isDOMException: true });
  });
});
