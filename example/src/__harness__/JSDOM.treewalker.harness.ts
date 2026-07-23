import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM TreeWalker / NodeIterator', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  // No whitespace between tags — Lexbor parses insignificant whitespace into
  // literal text nodes just like a real browser, and this suite's expected
  // node sequences assume there are none to account for.
  const TREE =
    '<html><body><div id="root">' +
    '<p id="p1">one</p>' +
    '<!-- a comment -->' +
    '<p id="p2">two<span id="s1">span</span></p>' +
    '<p id="p3">three</p>' +
    '</div></body></html>';

  it('createTreeWalker throws TypeError when root is not a Node', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      let caught;
      try { document.createTreeWalker('not a node'); } catch (e) { caught = e.constructor.name; }
      JSON.stringify({ caught });
    `);
    expect(JSON.parse(result)).toEqual({ caught: 'TypeError' });
  });

  it('nextNode() walks the whole subtree in tree order with SHOW_ALL, currentNode tracks the last visited node', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_ALL, null);
      const ids = [];
      let node, lastVisited;
      while ((node = walker.nextNode())) {
        lastVisited = node;
        ids.push(node.nodeType === 1 ? node.id || node.tagName : node.nodeType);
      }
      JSON.stringify({ ids, currentNodeIsLastVisited: walker.currentNode === lastVisited });
    `);
    expect(JSON.parse(result)).toEqual({
      ids: ['p1', 3, 8, 'p2', 3, 's1', 3, 'p3', 3],
      currentNodeIsLastVisited: true,
    });
  });

  it('SHOW_ELEMENT restricts traversal to elements only', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, null);
      const ids = [];
      let node;
      while ((node = walker.nextNode())) ids.push(node.id);
      JSON.stringify({ ids });
    `);
    expect(JSON.parse(result)).toEqual({ ids: ['p1', 'p2', 's1', 'p3'] });
  });

  it('a filter function returning FILTER_REJECT skips the whole subtree, SKIP visits children but not the node', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const rejectWalker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, function(node) {
        return node.id === 'p2' ? NodeFilter.FILTER_REJECT : NodeFilter.FILTER_ACCEPT;
      });
      const rejectIds = [];
      let node;
      while ((node = rejectWalker.nextNode())) rejectIds.push(node.id);

      const skipWalker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, function(node) {
        return node.id === 'p2' ? NodeFilter.FILTER_SKIP : NodeFilter.FILTER_ACCEPT;
      });
      const skipIds = [];
      while ((node = skipWalker.nextNode())) skipIds.push(node.id);

      JSON.stringify({ rejectIds, skipIds });
    `);
    expect(JSON.parse(result)).toEqual({
      rejectIds: ['p1', 'p3'],
      skipIds: ['p1', 's1', 'p3'],
    });
  });

  it('previousNode() reverses nextNode() exactly, and can land back on the walker root itself', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, null);
      const forwardIds = [];
      let node;
      while ((node = walker.nextNode())) forwardIds.push(node.id);
      const backwardIds = [];
      while ((node = walker.previousNode())) backwardIds.push(node.id);
      JSON.stringify({ forwardIds, backwardIds });
    `);
    expect(JSON.parse(result)).toEqual({
      forwardIds: ['p1', 'p2', 's1', 'p3'],
      // previousNode() walking past the first descendant lands on the
      // walker's own root — per spec, unlike nextNode() (which never
      // returns root, since traversal only ever moves into descendants).
      backwardIds: ['s1', 'p2', 'p1', 'root'],
    });
  });

  it('firstChild()/lastChild()/parentNode()/nextSibling()/previousSibling() navigate relative to currentNode', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, null);
      const first = walker.firstChild();
      const firstId = first.id;
      const next = walker.nextSibling();
      const nextId = next.id;
      const last = walker.lastChild();
      const lastId = last === null ? null : last.id;
      const backToParent = walker.parentNode();
      const parentIsP2 = backToParent.id;
      const currentAfterParent = walker.currentNode.id;
      JSON.stringify({ firstId, nextId, lastId, parentIsP2, currentAfterParent });
    `);
    expect(JSON.parse(result)).toEqual({
      firstId: 'p1', nextId: 'p2', lastId: 's1', parentIsP2: 'p2', currentAfterParent: 'p2',
    });
  });

  it('currentNode setter throws NotSupportedError DOMException when set to null', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const walker = document.createTreeWalker(root);
      let caught;
      try { walker.currentNode = null; } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
      JSON.stringify({ caught, whatToShowDefault: walker.whatToShow >>> 0 });
    `);
    expect(JSON.parse(result)).toEqual({
      caught: { name: 'NotSupportedError', isDOMException: true },
      whatToShowDefault: 0xFFFFFFFF,
    });
  });

  it('NodeIterator.nextNode() returns its own root first (unlike TreeWalker), then advances into descendants', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const it = document.createNodeIterator(root, NodeFilter.SHOW_ELEMENT, null);
      const initialRef = { node: it.referenceNode === root, before: it.pointerBeforeReferenceNode };
      const first = it.nextNode();
      const afterFirst = { id: first.id, ref: it.referenceNode === first, before: it.pointerBeforeReferenceNode };
      const second = it.nextNode();
      JSON.stringify({ initialRef, afterFirst, secondId: second.id });
    `);
    expect(JSON.parse(result)).toEqual({
      initialRef: { node: true, before: true },
      afterFirst: { id: 'root', ref: true, before: false },
      secondId: 'p1',
    });
  });

  it('previousNode() called right after nextNode() returns the same node (pointer flips without moving), a second call actually steps back', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const it = document.createNodeIterator(root, NodeFilter.SHOW_ELEMENT, null);
      it.nextNode(); // root
      const p1 = it.nextNode(); // p1 — real advance
      const stillP1 = it.previousNode(); // same node again, no movement
      const backToRoot = it.previousNode(); // now it actually steps back
      JSON.stringify({ p1Id: p1.id, stillP1Id: stillP1.id, sameNode: stillP1 === p1, backToRootId: backToRoot.id });
    `);
    expect(JSON.parse(result)).toEqual({
      p1Id: 'p1', stillP1Id: 'p1', sameNode: true, backToRootId: 'root',
    });
  });

  it('NodeIterator.detach() is a documented no-op — the iterator keeps working after calling it', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      const it = document.createNodeIterator(root, NodeFilter.SHOW_ELEMENT, null);
      it.detach();
      const node = it.nextNode();
      JSON.stringify({ id: node.id });
    `);
    expect(JSON.parse(result)).toEqual({ id: 'root' });
  });

  it('a recursive filter call throws InvalidStateError DOMException', async () => {
    dom = JSDOM.create(TREE);
    const result = await dom.evaluate(`
      const root = document.getElementById('root');
      let caught;
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT, function() {
        try { walker.nextNode(); } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
        return NodeFilter.FILTER_ACCEPT;
      });
      walker.nextNode();
      JSON.stringify({ caught });
    `);
    expect(JSON.parse(result)).toEqual({
      caught: { name: 'InvalidStateError', isDOMException: true },
    });
  });
});
