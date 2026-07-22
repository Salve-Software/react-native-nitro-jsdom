import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM native module', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('evaluates JS expressions inside the sandbox', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate('2 + 2');
    expect(result).toBe('4');
  });

  it('mutates and reads back DOM state', async () => {
    dom = JSDOM.create('<html><body><div id="result">0</div></body></html>');
    await dom.evaluate(`document.getElementById('result').textContent = String(2 + 2)`);
    const value = await dom.evaluate(`document.getElementById('result').textContent`);
    expect(value).toBe('4');
  });

  it('serialize() reflects mutations made via evaluate()', async () => {
    dom = JSDOM.create('<html><head></head><body></body></html>');
    await dom.evaluate(`document.title = 'Hello'`);
    expect(dom.serialize()).toContain('<title>Hello</title>');
  });

  it('rejects evaluate() after dispose()', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    dom.dispose();
    await expect(dom.evaluate('1')).rejects.toThrow('JSDOM instance has been disposed');
  });

  it('serialize() returns an empty string after dispose()', () => {
    dom = JSDOM.create('<html><body></body></html>');
    dom.dispose();
    expect(dom.serialize()).toBe('');
  });

  it('resolves setTimeout callbacks before evaluate() returns', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      (async () => {
        let value = 'not fired';
        setTimeout(() => { value = 'fired'; }, 10);
        await new Promise((resolve) => setTimeout(resolve, 20));
        return value;
      })()
    `);
    expect(result).toBe('fired');
  });

  it('forwards console output via onConsole', async () => {
    const captured: string[] = [];
    dom = JSDOM.create('<html><body></body></html>', {
      onConsole: (level, args) => captured.push(`${level}:${args.join(',')}`),
    });
    await dom.evaluate(`console.log('hello', 'world')`);
    expect(captured).toEqual(['log:hello,world']);
  });

  it('bridges window.confirm() to a synchronous onConfirm callback', async () => {
    dom = JSDOM.create('<html><body></body></html>', { onConfirm: () => true });
    const result = await dom.evaluate(`window.confirm('ok?')`);
    expect(result).toBe('true');
  });

  it('rejects fetch() when no onFetch handler is configured', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    await expect(dom.evaluate(`fetch('https://example.com')`)).rejects.toThrow();
  });

  it('bridges fetch() to a native onFetch handler round-trip', async () => {
    dom = JSDOM.create('<html><body></body></html>', {
      onFetch: async (url, init) => {
        expect(url).toBe('https://example.com/ping');
        expect(init.method).toBe('POST');
        expect(init.body).toBe('{"ping":true}');
        return {
          status: 201,
          statusText: 'Created',
          headers: { 'content-type': 'application/json' },
          body: '{"pong":true}',
        };
      },
    });

    const result = await dom.evaluate(`
      (async () => {
        const res = await fetch('https://example.com/ping', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ping: true }),
        });
        const json = await res.json();
        return JSON.stringify({ status: res.status, ok: res.ok, json });
      })()
    `);

    expect(JSON.parse(result)).toEqual({
      status: 201,
      ok: true,
      json: { pong: true },
    });
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

  it('cloneNode() shallow-clones by default and deep-clones with deep=true', async () => {
    dom = JSDOM.create('<html><body><div id="d" class="x"><span>hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const shallow = div.cloneNode();
      const deep = div.cloneNode(true);
      JSON.stringify({
        shallowClass: shallow.className,
        shallowChildCount: shallow.childNodes.length,
        deepChildCount: deep.childNodes.length,
        deepFirstChildTag: deep.firstChild.tagName,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      shallowClass: 'x',
      shallowChildCount: 0,
      deepChildCount: 1,
      deepFirstChildTag: 'SPAN',
    });
  });

  it('dataset reads and writes data-* attributes with camelCase mapping', async () => {
    dom = JSDOM.create('<html><body><div id="d" data-user-id="42" data-role="admin"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const before = { userId: div.dataset.userId, role: div.dataset.role };
      div.dataset.userId = '99';
      div.dataset.newFlag = 'yes';
      delete div.dataset.role;
      JSON.stringify({
        before,
        after: {
          userId: div.dataset.userId,
          newFlag: div.dataset.newFlag,
          role: div.dataset.role,
        },
        attrUserId: div.getAttribute('data-user-id'),
        attrNewFlag: div.getAttribute('data-new-flag'),
        attrRole: div.getAttribute('data-role'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { userId: '42', role: 'admin' },
      after: { userId: '99', newFlag: 'yes', role: undefined },
      attrUserId: '99',
      attrNewFlag: 'yes',
      attrRole: null,
    });
  });

  it('style reads/writes inline CSS via camelCase properties and cssText', async () => {
    dom = JSDOM.create('<html><body><div id="d" style="color: red; font-size: 12px;"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const before = { color: div.style.color, fontSize: div.style.fontSize };
      div.style.backgroundColor = 'blue';
      div.style.setProperty('margin-top', '4px');
      const afterGetPropertyValue = div.style.getPropertyValue('margin-top');
      div.style.removeProperty('color');
      JSON.stringify({
        before,
        backgroundColor: div.style.backgroundColor,
        afterGetPropertyValue,
        colorAfterRemove: div.style.color === undefined,
        cssText: div.style.cssText,
        attrStyle: div.getAttribute('style'),
      });
    `);
    const parsed = JSON.parse(result);
    expect(parsed.before).toEqual({ color: 'red', fontSize: '12px' });
    expect(parsed.backgroundColor).toBe('blue');
    expect(parsed.afterGetPropertyValue).toBe('4px');
    expect(parsed.colorAfterRemove).toBe(true);
    expect(parsed.cssText).toBe(parsed.attrStyle);
    expect(parsed.cssText).not.toContain('color: red');
    expect(parsed.cssText).toContain('background-color: blue');
    expect(parsed.cssText).toContain('margin-top: 4px');
  });

  it('dispatchEvent bubbles from target through ancestors to document', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><span id="child">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const order = [];
      const child = document.getElementById('child');
      const parent = document.getElementById('parent');
      child.addEventListener('click', () => order.push('child'));
      parent.addEventListener('click', () => order.push('parent'));
      document.addEventListener('click', () => order.push('document'));

      const event = new Event('click', { bubbles: true });
      child.dispatchEvent(event);
      JSON.stringify(order);
    `);
    expect(JSON.parse(result)).toEqual(['child', 'parent', 'document']);
  });

  it('dispatchEvent does not bubble when bubbles is false', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><span id="child">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const order = [];
      const child = document.getElementById('child');
      const parent = document.getElementById('parent');
      child.addEventListener('click', () => order.push('child'));
      parent.addEventListener('click', () => order.push('parent'));

      const event = new Event('click', { bubbles: false });
      child.dispatchEvent(event);
      JSON.stringify(order);
    `);
    expect(JSON.parse(result)).toEqual(['child']);
  });

  it('stopPropagation() halts bubbling to ancestors', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><span id="child">hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const order = [];
      const child = document.getElementById('child');
      const parent = document.getElementById('parent');
      child.addEventListener('click', (e) => { order.push('child'); e.stopPropagation(); });
      parent.addEventListener('click', () => order.push('parent'));

      const event = new Event('click', { bubbles: true });
      child.dispatchEvent(event);
      JSON.stringify(order);
    `);
    expect(JSON.parse(result)).toEqual(['child']);
  });

  it('preventDefault() sets defaultPrevented and dispatchEvent returns false when cancelable', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      div.addEventListener('click', (e) => e.preventDefault());
      const event = new Event('click', { cancelable: true });
      const returnValue = div.dispatchEvent(event);
      JSON.stringify({ returnValue, defaultPrevented: event.defaultPrevented });
    `);
    expect(JSON.parse(result)).toEqual({ returnValue: false, defaultPrevented: true });
  });

  it('preventDefault() has no effect when the event is not cancelable', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      div.addEventListener('click', (e) => e.preventDefault());
      const event = new Event('click', { cancelable: false });
      const returnValue = div.dispatchEvent(event);
      JSON.stringify({ returnValue, defaultPrevented: event.defaultPrevented });
    `);
    expect(JSON.parse(result)).toEqual({ returnValue: true, defaultPrevented: false });
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

  it('replaceChild() swaps a child and returns the removed node', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span id="old">old</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const oldEl = document.getElementById('old');
      const newEl = document.createElement('p');
      newEl.textContent = 'new';
      const returned = div.replaceChild(newEl, oldEl);
      JSON.stringify({ returnedIsOld: returned === oldEl, innerHTML: div.innerHTML });
    `);
    expect(JSON.parse(result)).toEqual({ returnedIsOld: true, innerHTML: '<p>new</p>' });
  });

  it('before()/after()/replaceWith() manipulate siblings, accepting nodes and strings', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span id="s">mid</span></div></body></html>');
    const result = await dom.evaluate(`
      const span = document.getElementById('s');
      span.before('before-text');
      span.after('after-text');
      document.getElementById('d').innerHTML;
    `);
    expect(result).toBe('before-text<span id="s">mid</span>after-text');

    const result2 = await dom.evaluate(`
      document.getElementById('s').replaceWith('replaced');
      document.getElementById('d').innerHTML;
    `);
    expect(result2).toBe('before-textreplacedafter-text');
  });

  it('append()/prepend() add children in argument order, accepting nodes and strings', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const b = document.createElement('b');
      b.textContent = 'b';
      div.append('a', b, 'c');
      div.prepend('z');
      div.innerHTML;
    `);
    expect(result).toBe('za<b>b</b>c');
  });

  it('insertAdjacentHTML() inserts parsed HTML at the four standard positions', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span>mid</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      div.insertAdjacentHTML('afterbegin', '<i>ab</i>');
      div.insertAdjacentHTML('beforeend', '<i>be</i>');
      div.insertAdjacentHTML('beforebegin', '<i>bb</i>');
      div.insertAdjacentHTML('afterend', '<i>ae</i>');
      document.body.innerHTML;
    `);
    expect(result).toBe(
      '<i>bb</i><div id="d"><i>ab</i><span>mid</span><i>be</i></div><i>ae</i>'
    );
  });

  it('document.createComment()/createDocumentFragment() create nodes usable with appendChild', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const comment = document.createComment('hi');
      div.appendChild(comment);

      const frag = document.createDocumentFragment();
      const a = document.createElement('a');
      a.textContent = 'link';
      frag.appendChild(a);
      div.appendChild(frag);

      JSON.stringify({
        commentNodeType: comment.nodeType,
        commentNodeValue: comment.nodeValue,
        innerHTML: div.innerHTML,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      commentNodeType: 8,
      commentNodeValue: 'hi',
      innerHTML: '<!--hi--><a>link</a>',
    });
  });

  it('getAttributeNames()/attributes list every attribute on the element', async () => {
    dom = JSDOM.create('<html><body><div id="d" class="x" data-role="admin"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      JSON.stringify({
        names: div.getAttributeNames(),
        attrs: Array.from(div.attributes).map((a) => [a.name, a.value]),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      names: ['id', 'class', 'data-role'],
      attrs: [
        ['id', 'd'],
        ['class', 'x'],
        ['data-role', 'admin'],
      ],
    });
  });

  it('toggleAttribute() toggles by default and respects the force argument', async () => {
    dom = JSDOM.create('<html><body><div id="d" hidden></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const removed = div.toggleAttribute('hidden');
      const afterRemove = div.hasAttribute('hidden');
      const added = div.toggleAttribute('hidden');
      const afterAdd = div.hasAttribute('hidden');
      const forcedFalseOnAbsent = div.toggleAttribute('missing', false);
      const forcedTrueTwice = [div.toggleAttribute('data-x', true), div.toggleAttribute('data-x', true)];
      JSON.stringify({ removed, afterRemove, added, afterAdd, forcedFalseOnAbsent, forcedTrueTwice });
    `);
    expect(JSON.parse(result)).toEqual({
      removed: false,
      afterRemove: false,
      added: true,
      afterAdd: true,
      forcedFalseOnAbsent: false,
      forcedTrueTwice: [true, true],
    });
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

  it('document.hidden defaults to true and is false when pretendToBeVisual is set', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const defaultResult = await dom.evaluate('String(document.hidden)');
    expect(defaultResult).toBe('true');
    dom.dispose();

    dom = JSDOM.create('<html><body></body></html>', { pretendToBeVisual: true });
    const visibleResult = await dom.evaluate('String(document.hidden)');
    expect(visibleResult).toBe('false');
  });

  it('evaluate() rejects with a clear error when called reentrantly from a callback', async () => {
    dom = JSDOM.create('<html><body></body></html>', {
      onFetch: async () => {
        await expect(dom!.evaluate('1')).rejects.toThrow();
        return { status: 200, body: '' };
      },
    });
    await dom.evaluate(`fetch('https://example.com')`);
  });

  it('skips non-JS <script> types like application/ld+json', async () => {
    dom = JSDOM.create(`
      <html>
        <body>
          <script>window.__ran = (window.__ran || 0) + 1;</script>
          <script type="application/ld+json">{"@type": "Thing"}</script>
          <script type="application/json">{"not": "js"}</script>
        </body>
      </html>
    `);
    const result = await dom.evaluate('window.__ran');
    expect(result).toBe('1');
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

  it('getBoundingClientRect() returns a zeroed rect instead of throwing', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify(document.getElementById('d').getBoundingClientRect());
    `);
    expect(JSON.parse(result)).toEqual({
      x: 0, y: 0, width: 0, height: 0, top: 0, right: 0, bottom: 0, left: 0,
    });
  });

  it('URL parses and serializes components, resolving relative URLs against a base', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const url = new URL('/path?a=1#hash', 'https://user:pass@example.com:8080');
      JSON.stringify({
        href: url.href,
        protocol: url.protocol,
        hostname: url.hostname,
        port: url.port,
        pathname: url.pathname,
        search: url.search,
        hash: url.hash,
        origin: url.origin,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      href: 'https://user:pass@example.com:8080/path?a=1#hash',
      protocol: 'https:',
      hostname: 'example.com',
      port: '8080',
      pathname: '/path',
      search: '?a=1',
      hash: '#hash',
      origin: 'https://example.com:8080',
    });
  });

  it('URL.searchParams stays in sync with url.search in both directions', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const url = new URL('https://example.com/?a=1');
      url.searchParams.append('b', '2');
      const afterAppend = url.search;
      url.search = '?c=3';
      const afterSearchSet = url.searchParams.get('c');
      JSON.stringify({ afterAppend, afterSearchSet, hasA: url.searchParams.has('a') });
    `);
    expect(JSON.parse(result)).toEqual({ afterAppend: '?a=1&b=2', afterSearchSet: '3', hasA: false });
  });

  it('URLSearchParams supports append/get/getAll/delete/set/sort/iteration', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const params = new URLSearchParams('b=2&a=1&a=3');
      const all = params.getAll('a');
      params.set('a', '9');
      const afterSet = params.getAll('a');
      params.delete('b');
      params.sort();
      JSON.stringify({ all, afterSet, toString: params.toString(), entries: Array.from(params) });
    `);
    expect(JSON.parse(result)).toEqual({
      all: ['1', '3'],
      afterSet: ['9'],
      toString: 'a=9',
      entries: [['a', '9']],
    });
  });

  it('AbortController.abort() marks the signal aborted and fires abort listeners', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const controller = new AbortController();
      const events = [];
      controller.signal.addEventListener('abort', () => events.push('listener'));
      controller.signal.onabort = () => events.push('onabort');
      const beforeAborted = controller.signal.aborted;
      controller.abort('custom reason');
      JSON.stringify({
        beforeAborted,
        afterAborted: controller.signal.aborted,
        reason: controller.signal.reason,
        events,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      beforeAborted: false,
      afterAborted: true,
      reason: 'custom reason',
      events: ['onabort', 'listener'],
    });
  });

  it('fetch() rejects immediately when called with an already-aborted signal', async () => {
    dom = JSDOM.create('<html><body></body></html>', {
      onFetch: async () => ({ status: 200, body: 'should not be reached' }),
    });
    await expect(
      dom.evaluate(`fetch('https://example.com', { signal: AbortSignal.abort() })`)
    ).rejects.toThrow();
  });

  it('requestAnimationFrame fires with a timestamp and can be cancelled', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      (async () => {
        let firedTimestamp = null;
        requestAnimationFrame((ts) => { firedTimestamp = ts; });

        const cancelledId = requestAnimationFrame(() => { throw new Error('should have been cancelled'); });
        cancelAnimationFrame(cancelledId);

        await new Promise((resolve) => setTimeout(resolve, 30));
        return JSON.stringify({ fired: typeof firedTimestamp === 'number' });
      })()
    `);
    expect(JSON.parse(result)).toEqual({ fired: true });
  });

  it('performance.now() returns increasing monotonic timestamps', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const a = performance.now();
      const b = performance.now();
      JSON.stringify({ isNumber: typeof a === 'number', increasing: b >= a });
    `);
    expect(JSON.parse(result)).toEqual({ isNumber: true, increasing: true });
  });

  it('queueMicrotask runs before a subsequently scheduled setTimeout', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      (async () => {
        const order = [];
        setTimeout(() => order.push('timeout'), 0);
        queueMicrotask(() => order.push('microtask'));
        await new Promise((resolve) => setTimeout(resolve, 10));
        return JSON.stringify(order);
      })()
    `);
    expect(JSON.parse(result)).toEqual(['microtask', 'timeout']);
  });

  it('crypto.getRandomValues fills a TypedArray and rejects BigInt64Array', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const a = new Uint8Array(8);
      crypto.getRandomValues(a);
      const allZero = a.every((v) => v === 0);
      const b = new Uint8Array(8);
      crypto.getRandomValues(b);
      const sameAsB = a.every((v, i) => v === b[i]);

      let threw = false;
      try { crypto.getRandomValues(new BigInt64Array(2)); } catch (e) { threw = true; }

      JSON.stringify({ allZero, sameAsB, threw, returnsSameRef: crypto.getRandomValues(a) === a });
    `);
    const parsed = JSON.parse(result);
    expect(parsed.allZero).toBe(false);
    expect(parsed.sameAsB).toBe(false);
    expect(parsed.threw).toBe(true);
    expect(parsed.returnsSameRef).toBe(true);
  });
});
