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

  it('createElementNS() with a prefixed qualified name exposes it via element.prefix', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const withPrefix = document.createElementNS('https://example.com/custom-ns', 'custom:widget');
      const withoutPrefix = document.createElement('div');
      JSON.stringify({ withPrefix: withPrefix.prefix, withoutPrefix: withoutPrefix.prefix });
    `);
    expect(JSON.parse(result)).toEqual({ withPrefix: 'custom', withoutPrefix: null });
  });

  it('setAttributeNS()/getAttributeNS()/hasAttributeNS()/removeAttributeNS() round-trip a namespaced attribute', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
      const XLINK = 'http://www.w3.org/1999/xlink';

      const beforeHas = use.hasAttributeNS(XLINK, 'href');
      const beforeGet = use.getAttributeNS(XLINK, 'href');

      use.setAttributeNS(XLINK, 'xlink:href', '#icon-star');
      const afterSet = { has: use.hasAttributeNS(XLINK, 'href'), value: use.getAttributeNS(XLINK, 'href') };

      use.setAttributeNS(XLINK, 'xlink:href', '#icon-heart');
      const afterUpdate = use.getAttributeNS(XLINK, 'href');

      use.removeAttributeNS(XLINK, 'href');
      const afterRemove = { has: use.hasAttributeNS(XLINK, 'href'), value: use.getAttributeNS(XLINK, 'href') };

      JSON.stringify({ beforeHas, beforeGet, afterSet, afterUpdate, afterRemove });
    `);
    expect(JSON.parse(result)).toEqual({
      beforeHas: false,
      beforeGet: null,
      afterSet: { has: true, value: '#icon-star' },
      afterUpdate: '#icon-heart',
      afterRemove: { has: false, value: null },
    });
  });

  it('setAttributeNS() is visible through plain getAttribute() using the qualified name', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
      use.setAttributeNS('http://www.w3.org/1999/xlink', 'xlink:href', '#icon-star');
      JSON.stringify({ plainGetAttribute: use.getAttribute('xlink:href') });
    `);
    expect(JSON.parse(result)).toEqual({ plainGetAttribute: '#icon-star' });
  });

  it('createElementNS(undefined, ...) stringifies to the literal "undefined" namespace, unlike null', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const el = document.createElementNS(undefined, 'div');
      JSON.stringify({ namespaceURI: el.namespaceURI });
    `);
    expect(JSON.parse(result)).toEqual({ namespaceURI: 'undefined' });
  });

  it('document.namespaceURI is null', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate('String(document.namespaceURI)');
    expect(result).toBe('null');
  });

  it('createElementNS() throws InvalidCharacterError for an empty or malformed qualified name', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      function tryCreate(ns, qname) {
        try {
          document.createElementNS(ns, qname);
          return null;
        } catch (e) {
          return { name: e.name, isDOMException: e instanceof DOMException };
        }
      }
      JSON.stringify({
        empty: tryCreate('http://www.w3.org/2000/svg', ''),
        tooManyColons: tryCreate('http://www.w3.org/2000/svg', 'a:b:c'),
        emptyPrefix: tryCreate('http://www.w3.org/2000/svg', ':foo'),
        emptyLocalName: tryCreate('http://www.w3.org/2000/svg', 'foo:'),
      });
    `);
    const expectedError = { name: 'InvalidCharacterError', isDOMException: true };
    expect(JSON.parse(result)).toEqual({
      empty: expectedError,
      tooManyColons: expectedError,
      emptyPrefix: expectedError,
      emptyLocalName: expectedError,
    });
  });

  it('createElementNS() throws NamespaceError for prefix/namespace and xml/xmlns inconsistencies', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      function tryCreate(ns, qname) {
        try {
          document.createElementNS(ns, qname);
          return null;
        } catch (e) {
          return { name: e.name, isDOMException: e instanceof DOMException };
        }
      }
      JSON.stringify({
        prefixWithoutNamespace: tryCreate(null, 'custom:widget'),
        xmlPrefixWrongNamespace: tryCreate('http://example.com/', 'xml:lang'),
        xmlPrefixRightNamespace: tryCreate('http://www.w3.org/XML/1998/namespace', 'xml:lang'),
        xmlnsQualifiedWrongNamespace: tryCreate('http://example.com/', 'xmlns'),
        xmlnsNamespaceWrongQualified: tryCreate('http://www.w3.org/2000/xmlns/', 'foo'),
        xmlnsNamespaceRightQualified: tryCreate('http://www.w3.org/2000/xmlns/', 'xmlns'),
      });
    `);
    const expectedError = { name: 'NamespaceError', isDOMException: true };
    expect(JSON.parse(result)).toEqual({
      prefixWithoutNamespace: expectedError,
      xmlPrefixWrongNamespace: expectedError,
      xmlPrefixRightNamespace: null,
      xmlnsQualifiedWrongNamespace: expectedError,
      xmlnsNamespaceWrongQualified: expectedError,
      xmlnsNamespaceRightQualified: null,
    });
  });

  it('node.isConnected reflects attachment to the document', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const el = document.createElement('div');
      const before = el.isConnected;
      document.body.appendChild(el);
      const afterAppend = el.isConnected;
      el.remove();
      const afterRemove = el.isConnected;
      JSON.stringify({ before, afterAppend, afterRemove, bodyIsConnected: document.body.isConnected });
    `);
    expect(JSON.parse(result)).toEqual({
      before: false, afterAppend: true, afterRemove: false, bodyIsConnected: true,
    });
  });

  it('lookupNamespaceURI()/isDefaultNamespace() resolve through xmlns attributes and ancestors', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const root = document.createElementNS('https://example.com/custom-ns', 'root');
      root.setAttribute('xmlns:foo', 'https://example.com/foo-ns');
      const child = document.createElement('child');
      root.appendChild(child);
      JSON.stringify({
        rootDefaultNs: root.lookupNamespaceURI(null),
        rootIsDefault: root.isDefaultNamespace('https://example.com/custom-ns'),
        childFooNs: child.lookupNamespaceURI('foo'),
        childUnknownPrefix: child.lookupNamespaceURI('bar'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      rootDefaultNs: 'https://example.com/custom-ns',
      rootIsDefault: true,
      childFooNs: 'https://example.com/foo-ns',
      childUnknownPrefix: null,
    });
  });

  it('lookupPrefix() finds the prefix declaring a given namespace on an ancestor', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const root = document.createElement('root');
      root.setAttribute('xmlns:foo', 'https://example.com/foo-ns');
      const child = document.createElement('child');
      root.appendChild(child);
      JSON.stringify({
        found: child.lookupPrefix('https://example.com/foo-ns'),
        notFound: child.lookupPrefix('https://example.com/nope'),
        nullNamespace: child.lookupPrefix(null),
      });
    `);
    expect(JSON.parse(result)).toEqual({ found: 'foo', notFound: null, nullNamespace: null });
  });
});
