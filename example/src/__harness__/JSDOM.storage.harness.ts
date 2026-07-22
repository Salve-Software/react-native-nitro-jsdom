import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM localStorage/sessionStorage', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('localStorage.setItem/getItem round-trips a value', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      localStorage.setItem('name', 'Ada');
      localStorage.getItem('name');
    `);
    expect(result).toBe('Ada');
  });

  it('localStorage.getItem returns null for a missing key', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`String(localStorage.getItem('missing'))`);
    expect(result).toBe('null');
  });

  it('localStorage.removeItem and length track insertion order', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      localStorage.setItem('a', '1');
      localStorage.setItem('b', '2');
      const before = localStorage.length;
      localStorage.removeItem('a');
      JSON.stringify({ before, after: localStorage.length, key0: localStorage.key(0) });
    `);
    expect(JSON.parse(result)).toEqual({ before: 2, after: 1, key0: 'b' });
  });

  it('localStorage.clear() empties the store', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      localStorage.setItem('x', '1');
      localStorage.clear();
      String(localStorage.length)
    `);
    expect(result).toBe('0');
  });

  it('localStorage and sessionStorage are independent stores', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      localStorage.setItem('shared', 'nope');
      String(sessionStorage.getItem('shared'))
    `);
    expect(result).toBe('null');
  });
});
