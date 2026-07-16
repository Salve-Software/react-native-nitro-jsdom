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
});
