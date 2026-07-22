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

  it('Element/Node/Document constructors throw when called directly', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let threw = 0;
      try { new Element(); } catch (e) { threw++; }
      try { new Node(); } catch (e) { threw++; }
      try { new Document(); } catch (e) { threw++; }
      threw;
    `);
    expect(result).toBe('3');
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
});
