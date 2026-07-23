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

  it('normalize() merges adjacent text nodes and drops empty ones', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      div.appendChild(document.createTextNode('a'));
      div.appendChild(document.createTextNode('b'));
      div.appendChild(document.createTextNode(''));
      div.appendChild(document.createTextNode('c'));
      div.normalize();
      JSON.stringify({
        childNodes: div.childNodes.length,
        text: div.firstChild.textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({ childNodes: 1, text: 'abc' });
  });

  it('normalize() removes an all-empty run of adjacent text nodes entirely', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      div.appendChild(document.createTextNode(''));
      div.appendChild(document.createTextNode(''));
      div.normalize();
      JSON.stringify({ childNodes: div.childNodes.length });
    `);
    expect(JSON.parse(result)).toEqual({ childNodes: 0 });
  });

  it('normalize() recurses into descendant elements without touching unrelated siblings', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span id="s"></span></div></body></html>');
    const result = await dom.evaluate(`
      const span = document.getElementById('s');
      span.appendChild(document.createTextNode('x'));
      span.appendChild(document.createTextNode('y'));
      document.getElementById('d').normalize();
      JSON.stringify({ spanChildNodes: span.childNodes.length, spanText: span.textContent });
    `);
    expect(JSON.parse(result)).toEqual({ spanChildNodes: 1, spanText: 'xy' });
  });

  it('compareDocumentPosition() reports ancestor/descendant relationships', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><span id="child">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const parent = document.getElementById('parent');
      const child = document.getElementById('child');
      const CONTAINS = 8, CONTAINED_BY = 16, PRECEDING = 2, FOLLOWING = 4;
      JSON.stringify({
        parentVsChild: parent.compareDocumentPosition(child),
        childVsParent: child.compareDocumentPosition(parent),
        expectedParentVsChild: CONTAINED_BY | FOLLOWING,
        expectedChildVsParent: CONTAINS | PRECEDING,
        selfVsSelf: parent.compareDocumentPosition(parent),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      parentVsChild: 20,
      childVsParent: 10,
      expectedParentVsChild: 20,
      expectedChildVsParent: 10,
      selfVsSelf: 0,
    });
  });

  it('compareDocumentPosition() reports PRECEDING/FOLLOWING for sibling subtrees', async () => {
    dom = JSDOM.create('<html><body><div id="a"></div><div id="b"></div></body></html>');
    const result = await dom.evaluate(`
      const a = document.getElementById('a');
      const b = document.getElementById('b');
      JSON.stringify({
        aVsB: a.compareDocumentPosition(b),
        bVsA: b.compareDocumentPosition(a),
      });
    `);
    expect(JSON.parse(result)).toEqual({ aVsB: 4, bVsA: 2 });
  });

  it('compareDocumentPosition() reports DISCONNECTED for nodes in different trees', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const detached = document.createElement('span');
      const position = div.compareDocumentPosition(detached);
      const DISCONNECTED = 1;
      JSON.stringify({ isDisconnected: (position & DISCONNECTED) === DISCONNECTED });
    `);
    expect(JSON.parse(result)).toEqual({ isDisconnected: true });
  });

  it('Node exposes the WHATWG numeric type constants as static properties', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        staticElement: Node.ELEMENT_NODE,
        staticText: Node.TEXT_NODE,
        staticComment: Node.COMMENT_NODE,
        staticDocument: Node.DOCUMENT_NODE,
        staticDocumentFragment: Node.DOCUMENT_FRAGMENT_NODE,
        DocumentStatic: Document.DOCUMENT_NODE,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      staticElement: 1,
      staticText: 3,
      staticComment: 8,
      staticDocument: 9,
      staticDocumentFragment: 11,
      DocumentStatic: 9,
    });
  });

  it('node instances inherit the type constants and can compare nodeType against them', async () => {
    dom = JSDOM.create('<html><body><div id="d">hi<!--c--></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const text = div.firstChild;
      const comment = div.lastChild;
      const fragment = document.createDocumentFragment();
      JSON.stringify({
        elMatches: div.nodeType === Node.ELEMENT_NODE,
        textMatches: text.nodeType === Node.TEXT_NODE,
        commentMatches: comment.nodeType === Node.COMMENT_NODE,
        fragmentMatches: fragment.nodeType === Node.DOCUMENT_FRAGMENT_NODE,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      elMatches: true,
      textMatches: true,
      commentMatches: true,
      fragmentMatches: true,
    });
  });

  it('document exposes nodeType/nodeName and the type constants as a shortcut', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        docStatic: document.ELEMENT_NODE,
        docNodeType: document.nodeType,
        docNodeName: document.nodeName,
      });
    `);
    expect(JSON.parse(result)).toEqual({ docStatic: 1, docNodeType: 9, docNodeName: '#document' });
  });

  it('getRootNode() of an attached element is the document, matching the attachment-check idiom', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><span id="child">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const child = document.getElementById('child');
      child.getRootNode() === document;
    `);
    expect(result).toBe('true');
  });

  it('getRootNode() on a detached node returns itself', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const detached = document.createElement('div');
      detached.getRootNode() === detached;
    `);
    expect(result).toBe('true');
  });

  it('ownerDocument resolves to the real document global for primary-document nodes, and to null for the document itself', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const d = document.getElementById('d');
      JSON.stringify({
        isDocument: d.ownerDocument === document,
        detachedIsDocument: document.createElement('span').ownerDocument === document,
        documentOwnerDocument: document.ownerDocument,
      });
    `);
    expect(JSON.parse(result)).toEqual({ isDocument: true, detachedIsDocument: true, documentOwnerDocument: null });
  });

  it('ownerDocument of a node from a secondary document resolves to that document, not the primary one', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const parsed = new DOMParser().parseFromString('<html><body><p id="x"></p></body></html>', 'text/html');
      const p = parsed.getElementById('x');
      JSON.stringify({
        isParsedDoc: p.ownerDocument === parsed,
        isPrimaryDoc: p.ownerDocument === document,
      });
    `);
    expect(JSON.parse(result)).toEqual({ isParsedDoc: true, isPrimaryDoc: false });
  });

  it('CharacterData.data mirrors nodeValue on Text/Comment nodes, .length is the JS string length', async () => {
    dom = JSDOM.create('<html><body><p id="p">café</p><!--café--></body></html>');
    const result = await dom.evaluate(`
      const text = document.getElementById('p').firstChild;
      const comment = document.body.childNodes[document.body.childNodes.length - 1];
      const before = { data: text.data, length: text.length };
      text.data = 'naïve';
      const after = { data: text.data, nodeValue: text.nodeValue, length: text.length };
      JSON.stringify({
        before, after,
        commentData: comment.data, commentLength: comment.length,
        elementDataIsNull: document.getElementById('p').data === null,
        elementLengthIsUndefined: document.getElementById('p').length === undefined,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { data: 'café', length: 4 },
      after: { data: 'naïve', nodeValue: 'naïve', length: 5 },
      commentData: 'café', commentLength: 4,
      elementDataIsNull: true,
      elementLengthIsUndefined: true,
    });
  });

  it('innerText falls back to textContent (no layout engine to compute rendered text)', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span>a</span> b </div></body></html>');
    const result = await dom.evaluate(`
      const d = document.getElementById('d');
      const before = d.innerText;
      d.innerText = 'replaced';
      JSON.stringify({ before, after: d.innerText, textContentAfter: d.textContent });
    `);
    expect(JSON.parse(result)).toEqual({ before: 'a b ', after: 'replaced', textContentAfter: 'replaced' });
  });
});
