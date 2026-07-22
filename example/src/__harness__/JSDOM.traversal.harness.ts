import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM node traversal', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('nodeType/nodeName report the correct values for element, text, and document nodes', async () => {
    dom = JSDOM.create('<html><body><p id="p">hi</p></body></html>');
    const result = await dom.evaluate(`
      const p = document.getElementById('p');
      const text = p.firstChild;
      JSON.stringify({
        elNodeType: p.nodeType,
        elNodeName: p.nodeName,
        textNodeType: text.nodeType,
        textNodeName: text.nodeName,
        docNodeType: document.documentElement.parentNode.nodeType,
        docNodeName: document.documentElement.parentNode.nodeName,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      elNodeType: 1,
      elNodeName: 'P',
      textNodeType: 3,
      textNodeName: '#text',
      docNodeType: 9,
      docNodeName: '#document',
    });
  });

  it('nodeValue reads and writes text node data', async () => {
    dom = JSDOM.create('<html><body><p id="p">hi</p></body></html>');
    const result = await dom.evaluate(`
      const text = document.getElementById('p').firstChild;
      const before = text.nodeValue;
      text.nodeValue = 'bye';
      JSON.stringify({ before, after: text.nodeValue, textContent: document.getElementById('p').textContent });
    `);
    expect(JSON.parse(result)).toEqual({ before: 'hi', after: 'bye', textContent: 'bye' });
  });

  it('nodeValue is null for element nodes', async () => {
    dom = JSDOM.create('<html><body><p id="p">hi</p></body></html>');
    const result = await dom.evaluate(`String(document.getElementById('p').nodeValue)`);
    expect(result).toBe('null');
  });

  it('childNodes includes all child node types in document order', async () => {
    dom = JSDOM.create('<html><body><div id="d">a<span>b</span>c</div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      JSON.stringify(Array.from(div.childNodes).map((n) => ({ type: n.nodeType, name: n.nodeName })));
    `);
    expect(JSON.parse(result)).toEqual([
      { type: 3, name: '#text' },
      { type: 1, name: 'SPAN' },
      { type: 3, name: '#text' },
    ]);
  });

  it('firstChild/lastChild/nextSibling/previousSibling walk the node tree', async () => {
    dom = JSDOM.create('<html><body><div id="d">a<span>b</span>c</div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const first = div.firstChild;
      const span = first.nextSibling;
      const last = div.lastChild;
      JSON.stringify({
        firstNodeValue: first.nodeValue,
        spanNodeName: span.nodeName,
        lastNodeValue: last.nodeValue,
        spanPrevNodeValue: span.previousSibling.nodeValue,
        lastPrevNodeName: last.previousSibling.nodeName,
        noNextOnLast: last.nextSibling === null,
        noPrevOnFirst: first.previousSibling === null,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      firstNodeValue: 'a',
      spanNodeName: 'SPAN',
      lastNodeValue: 'c',
      spanPrevNodeValue: 'a',
      lastPrevNodeName: 'SPAN',
      noNextOnLast: true,
      noPrevOnFirst: true,
    });
  });

  it('parentNode walks up to the parent element', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span id="s">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const span = document.getElementById('s');
      JSON.stringify({
        parentId: span.parentNode.id,
        grandparentTag: span.parentNode.parentNode.tagName,
      });
    `);
    expect(JSON.parse(result)).toEqual({ parentId: 'd', grandparentTag: 'BODY' });
  });

  it('node identity is stable across repeated lookups of the same node', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span>hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      JSON.stringify({
        sameElement: document.getElementById('d') === div,
        sameFirstChild: div.firstChild === div.firstChild,
      });
    `);
    expect(JSON.parse(result)).toEqual({ sameElement: true, sameFirstChild: true });
  });

  it('contains() checks ancestor/descendant relationships', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><span id="child">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const parent = document.getElementById('parent');
      const child = document.getElementById('child');
      JSON.stringify({
        parentContainsChild: parent.contains(child),
        childContainsParent: child.contains(parent),
        selfContains: parent.contains(parent),
      });
    `);
    expect(JSON.parse(result)).toEqual({ parentContainsChild: true, childContainsParent: false, selfContains: true });
  });

  it('closest() walks up matching ancestors', async () => {
    dom = JSDOM.create('<html><body><div class="outer"><div class="inner"><span id="s">hi</span></div></div></body></html>');
    const result = await dom.evaluate(`
      const span = document.getElementById('s');
      JSON.stringify({
        inner: span.closest('.inner') !== null,
        outer: span.closest('.outer') !== null,
        none: span.closest('.missing'),
      });
    `);
    expect(JSON.parse(result)).toEqual({ inner: true, outer: true, none: null });
  });

  it('isSameNode() checks identity, isEqualNode() checks structural equality', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span class="x">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const span = div.firstChild;
      const clone = span.cloneNode(true);
      const different = document.createElement('span');
      different.className = 'y';
      JSON.stringify({
        sameNodeSelf: span.isSameNode(div.firstChild),
        sameNodeClone: span.isSameNode(clone),
        equalNodeClone: span.isEqualNode(clone),
        equalNodeDifferent: span.isEqualNode(different),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      sameNodeSelf: true,
      sameNodeClone: false,
      equalNodeClone: true,
      equalNodeDifferent: false,
    });
  });
});
