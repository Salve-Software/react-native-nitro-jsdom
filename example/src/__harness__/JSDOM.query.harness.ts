import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM selectors, live collections, instanceof', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('document.getElementsByTagName finds all matching elements in document order', async () => {
    dom = JSDOM.create('<html><body><p>a</p><div><p>b</p></div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.getElementsByTagName('p')).map((p) => p.textContent));
    `);
    expect(JSON.parse(result)).toEqual(['a', 'b']);
  });

  it('document.getElementsByClassName finds elements having all listed classes', async () => {
    dom = JSDOM.create(`
      <html><body>
        <p class="foo bar">a</p>
        <p class="foo">b</p>
        <p class="foo bar baz">c</p>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.getElementsByClassName('foo bar')).map((p) => p.textContent));
    `);
    expect(JSON.parse(result)).toEqual(['a', 'c']);
  });

  it('element.getElementsByTagName/getElementsByClassName scope to a subtree', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="scope"><p class="hit">a</p></div>
        <p class="hit">outside</p>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const scope = document.getElementById('scope');
      JSON.stringify({
        byTag: Array.from(scope.getElementsByTagName('p')).map((p) => p.textContent),
        byClass: Array.from(scope.getElementsByClassName('hit')).map((p) => p.textContent),
      });
    `);
    expect(JSON.parse(result)).toEqual({ byTag: ['a'], byClass: ['a'] });
  });

  it('exposes Node/Element/HTMLElement/Document globals for instanceof checks', async () => {
    dom = JSDOM.create('<html><body><div id="d">hi</div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const text = div.firstChild;
      JSON.stringify({
        elInstanceofElement: div instanceof Element,
        elInstanceofNode: div instanceof Node,
        elInstanceofHTMLElement: div instanceof HTMLElement,
        textInstanceofNode: text instanceof Node,
        textInstanceofElement: text instanceof Element,
        docInstanceofDocument: document instanceof Document,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      elInstanceofElement: true,
      elInstanceofNode: true,
      elInstanceofHTMLElement: true,
      textInstanceofNode: true,
      textInstanceofElement: false,
      docInstanceofDocument: true,
    });
  });

  it('Element/Node constructors throw when called directly', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let threw = 0;
      try { new Element(); } catch (e) { threw++; }
      try { new Node(); } catch (e) { threw++; }
      threw;
    `);
    expect(result).toBe('2');
  });

  it('children/childNodes/getElementsBy* reflect DOM mutations made after the initial lookup', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const children = div.children;
      const childNodes = div.childNodes;
      const byTag = document.getElementsByTagName('span');
      const before = { children: children.length, childNodes: childNodes.length, byTag: byTag.length };

      const span = document.createElement('span');
      div.appendChild(span);

      JSON.stringify({
        before,
        after: { children: children.length, childNodes: childNodes.length, byTag: byTag.length },
        childrenItem0Tag: children[0].tagName,
        iterated: Array.from(children).map((el) => el.tagName),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { children: 0, childNodes: 0, byTag: 0 },
      after: { children: 1, childNodes: 1, byTag: 1 },
      childrenItem0Tag: 'SPAN',
      iterated: ['SPAN'],
    });
  });

  it('querySelectorAll returns a static snapshot, unlike getElementsBy*', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const snapshot = document.querySelectorAll('span');
      div.appendChild(document.createElement('span'));
      snapshot.length;
    `);
    expect(result).toBe('0');
  });

  it('document.images/.scripts finds all matching elements in document order', async () => {
    dom = JSDOM.create(`
      <html><body>
        <img id="i1" src="a.png">
        <script>void 0;</script>
        <div><img id="i2" src="b.png"></div>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify({
        imageIds: Array.from(document.images).map((el) => el.id),
        scriptCount: document.scripts.length,
      });
    `);
    expect(JSON.parse(result)).toEqual({ imageIds: ['i1', 'i2'], scriptCount: 1 });
  });

  it('document.links finds <a>/<area> only when they have an href attribute', async () => {
    dom = JSDOM.create(`
      <html><body>
        <a id="withHref" href="/x">x</a>
        <a id="noHref">no href</a>
        <area id="areaWithHref" href="/y">
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.links).map((el) => el.id));
    `);
    expect(JSON.parse(result)).toEqual(['withHref', 'areaWithHref']);
  });

  it('document.forms finds all <form> elements', async () => {
    dom = JSDOM.create('<html><body><form id="f1"></form><div><form id="f2"></form></div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.forms).map((el) => el.id));
    `);
    expect(JSON.parse(result)).toEqual(['f1', 'f2']);
  });

  it('document.getElementsByName finds elements by their name attribute', async () => {
    dom = JSDOM.create(`
      <html><body>
        <input name="choice" id="a">
        <input name="choice" id="b">
        <input name="other" id="c">
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.getElementsByName('choice')).map((el) => el.id));
    `);
    expect(JSON.parse(result)).toEqual(['a', 'b']);
  });

  it('document.doctype exposes name/publicId/systemId, or null when absent', async () => {
    dom = JSDOM.create('<!DOCTYPE html><html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        name: document.doctype.name,
        publicId: document.doctype.publicId,
        systemId: document.doctype.systemId,
        nodeType: document.doctype.nodeType,
      });
    `);
    expect(JSON.parse(result)).toEqual({ name: 'html', publicId: '', systemId: '', nodeType: 10 });

    dom.dispose();
    dom = JSDOM.create('<html><body></body></html>');
    const noDoctype = await dom.evaluate('String(document.doctype)');
    expect(noDoctype).toBe('null');
  });

  it('document.doctype returns the same object on repeated access', async () => {
    dom = JSDOM.create('<!DOCTYPE html><html><body></body></html>');
    const result = await dom.evaluate('document.doctype === document.doctype');
    expect(result).toBe('true');
  });

  it('querySelectorAll()/getElementsBy*() results support forEach(value, index, collection)', async () => {
    dom = JSDOM.create(`
      <html><body>
        <p class="hit">a</p>
        <p class="hit">b</p>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const seen = [];
      document.querySelectorAll('.hit').forEach((el, i, collection) => {
        seen.push({ text: el.textContent, index: i, collectionLength: collection.length });
      });
      JSON.stringify(seen);
    `);
    expect(JSON.parse(result)).toEqual([
      { text: 'a', index: 0, collectionLength: 2 },
      { text: 'b', index: 1, collectionLength: 2 },
    ]);
  });

  it('querySelectorAll()/getElementsBy*() results support entries()/keys()/values()', async () => {
    dom = JSDOM.create(`
      <html><body>
        <p class="hit">a</p>
        <p class="hit">b</p>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const list = document.querySelectorAll('.hit');
      const entries = Array.from(list.entries()).map(([i, el]) => [i, el.textContent]);
      const keys = Array.from(list.keys());
      const values = Array.from(list.values()).map((el) => el.textContent);
      JSON.stringify({ entries, keys, values });
    `);
    expect(JSON.parse(result)).toEqual({
      entries: [[0, 'a'], [1, 'b']],
      keys: [0, 1],
      values: ['a', 'b'],
    });
  });
});
