import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM namespaces (createElementNS/namespaceURI)', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('createElement() (no namespace) produces an HTML element with the XHTML namespace and uppercase tagName', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const el = document.createElement('div');
      JSON.stringify({ tagName: el.tagName, namespaceURI: el.namespaceURI });
    `);
    expect(JSON.parse(result)).toEqual({
      tagName: 'DIV',
      namespaceURI: 'http://www.w3.org/1999/xhtml',
    });
  });

  it('createElementNS() with the SVG namespace produces an element with a lowercase tagName', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      svg.appendChild(circle);
      document.body.appendChild(svg);
      JSON.stringify({
        svgTagName: svg.tagName,
        svgNamespaceURI: svg.namespaceURI,
        circleNamespaceURI: circle.namespaceURI,
        serialized: document.body.innerHTML,
      });
    `);
    const parsed = JSON.parse(result);
    expect(parsed.svgTagName).toBe('svg');
    expect(parsed.svgNamespaceURI).toBe('http://www.w3.org/2000/svg');
    expect(parsed.circleNamespaceURI).toBe('http://www.w3.org/2000/svg');
    expect(parsed.serialized).toContain('<svg>');
    expect(parsed.serialized).toContain('<circle>');
  });

  it('createElementNS() supports an arbitrary custom namespace URI and a prefixed qualified name', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const el = document.createElementNS('https://example.com/custom-ns', 'custom:widget');
      JSON.stringify({ namespaceURI: el.namespaceURI, tagName: el.tagName });
    `);
    expect(JSON.parse(result)).toEqual({
      namespaceURI: 'https://example.com/custom-ns',
      tagName: 'custom:widget',
    });
  });

  it('createElementNS() with a null namespace produces an element with no namespaceURI', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const el = document.createElementNS(null, 'foo');
      JSON.stringify({ namespaceURI: el.namespaceURI });
    `);
    expect(JSON.parse(result)).toEqual({ namespaceURI: null });
  });

  it('createElementNS() requires both arguments', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let caught;
      try { document.createElementNS('http://www.w3.org/2000/svg'); }
      catch (e) { caught = e.constructor.name; }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toBe('TypeError');
  });
});
