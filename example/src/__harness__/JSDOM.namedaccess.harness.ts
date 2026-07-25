import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM named access on window', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('an element with an id already in the parsed document is reachable as a bare global', async () => {
    dom = JSDOM.create('<html><body><div id="countdown">3 days left</div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        bare: countdown.textContent,
        viaWindow: window.countdown.textContent,
        sameNode: countdown === document.getElementById('countdown'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      bare: '3 days left',
      viaWindow: '3 days left',
      sameNode: true,
    });
  });

  it('a "name" attribute is exposed only for embed/form/img/object/iframe/frame, not arbitrary elements', async () => {
    dom = JSDOM.create(`
      <html><body>
        <form name="myForm"></form>
        <img name="myImg">
        <span name="myShouldNotAppear"></span>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify({
        form: typeof myForm !== 'undefined' && myForm.tagName,
        img: typeof myImg !== 'undefined' && myImg.tagName,
        span: typeof myShouldNotAppear,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      form: 'FORM',
      img: 'IMG',
      span: 'undefined',
    });
  });

  it('a dynamically created element is not reachable before being connected', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const a = document.createElement('div');
      a.id = 'widgetA';
      typeof widgetA;
    `);
    expect(result).toBe('undefined');
  });

  it('appendChild() connects a dynamically created element, and it resolves to the same object', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const a = document.createElement('div');
      a.id = 'widgetA';
      document.body.appendChild(a);
      JSON.stringify({ typeofWidgetA: typeof widgetA, sameObject: typeof widgetA !== 'undefined' && widgetA === a });
    `);
    expect(JSON.parse(result)).toEqual({ typeofWidgetA: 'object', sameObject: true });
  });

  it('removeChild() disconnects a dynamically created element', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const a = document.createElement('div');
      a.id = 'widgetA';
      document.body.appendChild(a);
      document.body.removeChild(a);
      typeof widgetA;
    `);
    expect(result).toBe('undefined');
  });

  it('element.remove() disconnects a dynamically created element', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const b = document.createElement('div');
      b.id = 'widgetB';
      document.body.appendChild(b);
      const beforeRemove = typeof widgetB;
      b.remove();
      JSON.stringify({ beforeRemove, afterRemove: typeof widgetB });
    `);
    expect(JSON.parse(result)).toEqual({ beforeRemove: 'object', afterRemove: 'undefined' });
  });

  it('innerHTML assignment disconnects the old subtree and connects the new one', async () => {
    dom = JSDOM.create('<html><body><div id="container"><span id="old">old</span></div></body></html>');
    const result = await dom.evaluate(`
      const before = typeof old;
      container.innerHTML = '<span id="fresh">fresh</span>';
      JSON.stringify({ before, afterOld: typeof old, fresh: typeof fresh !== 'undefined' && fresh.textContent });
    `);
    expect(JSON.parse(result)).toEqual({ before: 'object', afterOld: 'undefined', fresh: 'fresh' });
  });

  it('insertAdjacentHTML connects newly inserted elements', async () => {
    dom = JSDOM.create('<html><body><div id="container"></div></body></html>');
    const result = await dom.evaluate(`
      container.insertAdjacentHTML('beforeend', '<span id="badge">-20%</span>');
      JSON.stringify(typeof badge !== 'undefined' && badge.textContent);
    `);
    expect(JSON.parse(result)).toBe('-20%');
  });

  it("changing an element's id via setAttribute()/removeAttribute() updates which global resolves", async () => {
    dom = JSDOM.create('<html><body><div id="oldName">hi</div></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      log.push(typeof oldName);
      oldName.setAttribute('id', 'newName');
      log.push(typeof oldName, typeof newName, newName.textContent);
      newName.removeAttribute('id');
      log.push(typeof newName);
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['object', 'undefined', 'object', 'hi', 'undefined']);
  });

  it('duplicate ids resolve to a plain array of the matching elements', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="dup" class="first">a</div>
        <div id="dup" class="second">b</div>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify({
        isArray: Array.isArray(dup),
        length: dup.length,
        classes: dup.map((el) => el.className),
      });
    `);
    expect(JSON.parse(result)).toEqual({ isArray: true, length: 2, classes: ['first', 'second'] });
  });

  it('never overwrites a key that already exists as a real global', async () => {
    dom = JSDOM.create('<html><body><div id="document">not the real document</div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        stillRealDocument: document.nodeType === 9,
        foundViaQuery: document.getElementById('document').textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({ stillRealDocument: true, foundViaQuery: 'not the real document' });
  });

  it('elements inside a shadow root are not exposed as named globals', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const root = host.attachShadow({ mode: 'open' });
      root.innerHTML = '<span id="insideShadow">hi</span>';
      JSON.stringify(typeof insideShadow);
    `);
    expect(JSON.parse(result)).toBe('undefined');
  });

  it('documented limitation: insertBefore() does not register the inserted element (only appendChild/removeChild/remove/innerHTML/insertAdjacentHTML/setAttribute are hooked)', async () => {
    dom = JSDOM.create('<html><body><div id="container"><span id="ref">ref</span></div></body></html>');
    const result = await dom.evaluate(`
      const el = document.createElement('div');
      el.id = 'notHooked';
      container.insertBefore(el, ref);
      JSON.stringify(typeof notHooked);
    `);
    expect(JSON.parse(result)).toBe('undefined');
  });
});
