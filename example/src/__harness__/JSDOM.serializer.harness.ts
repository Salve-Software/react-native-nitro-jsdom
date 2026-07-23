import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM XMLSerializer', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('serializeToString(element) returns the element and its subtree as markup', async () => {
    dom = JSDOM.create('<html><body><div id="d"><p>hi</p></div></body></html>');
    const result = await dom.evaluate(`
      const serializer = new XMLSerializer();
      serializer.serializeToString(document.getElementById('d'));
    `);
    expect(result).toBe('<div id="d"><p>hi</p></div>');
  });

  it('serializeToString(document.documentElement) serializes the whole document tree', async () => {
    dom = JSDOM.create('<html><head></head><body><span>x</span></body></html>');
    const result = await dom.evaluate(`
      new XMLSerializer().serializeToString(document.documentElement);
    `);
    expect(result).toContain('<span>x</span>');
    expect(result.startsWith('<html>')).toBe(true);
  });
});
