import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM TextEncoder/TextDecoder', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('encodes and decodes ASCII round-trip', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const bytes = new TextEncoder().encode('hello');
      JSON.stringify({
        isUint8Array: bytes instanceof Uint8Array,
        bytes: Array.from(bytes),
        decoded: new TextDecoder().decode(bytes),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      isUint8Array: true,
      bytes: [104, 101, 108, 108, 111],
      decoded: 'hello',
    });
  });

  it('round-trips multi-byte UTF-8 (including a surrogate pair) correctly', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const original = 'café 🎉';
      const bytes = new TextEncoder().encode(original);
      new TextDecoder().decode(bytes);
    `);
    expect(result).toBe('café 🎉');
  });
});
