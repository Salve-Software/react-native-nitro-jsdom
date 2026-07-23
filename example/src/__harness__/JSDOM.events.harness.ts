import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM events', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
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

  it('document.removeEventListener() stops a previously registered listener from firing', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      function handler() { log.push('fired'); }
      document.addEventListener('custom', handler);
      document.dispatchEvent(new Event('custom'));
      document.removeEventListener('custom', handler);
      document.dispatchEvent(new Event('custom'));
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['fired']);
  });

  it('window.removeEventListener() stops a previously registered listener from firing', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      function handler() { log.push('fired'); }
      window.addEventListener('custom', handler);
      window.dispatchEvent(new Event('custom'));
      window.removeEventListener('custom', handler);
      window.dispatchEvent(new Event('custom'));
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['fired']);
  });

  it('el.onclick fires on click and reassigning replaces the previous handler', async () => {
    dom = JSDOM.create('<html><body><button id="b"></button></body></html>');
    const result = await dom.evaluate(`
      const btn = document.getElementById('b');
      const log = [];
      btn.onclick = () => log.push('first');
      btn.dispatchEvent(new Event('click'));
      btn.onclick = () => log.push('second');
      btn.dispatchEvent(new Event('click'));
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['first', 'second']);
  });

  it('el.onclick coexists with addEventListener listeners for the same event type', async () => {
    dom = JSDOM.create('<html><body><button id="b"></button></body></html>');
    const result = await dom.evaluate(`
      const btn = document.getElementById('b');
      const log = [];
      btn.addEventListener('click', () => log.push('listener'));
      btn.onclick = () => log.push('handler');
      btn.dispatchEvent(new Event('click'));
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['listener', 'handler']);
  });

  it('setting el.onclick to null removes the previously assigned handler', async () => {
    dom = JSDOM.create('<html><body><button id="b"></button></body></html>');
    const result = await dom.evaluate(`
      const btn = document.getElementById('b');
      const log = [];
      btn.onclick = () => log.push('fired');
      btn.onclick = null;
      btn.dispatchEvent(new Event('click'));
      JSON.stringify({ log, onclick: btn.onclick });
    `);
    expect(JSON.parse(result)).toEqual({ log: [], onclick: null });
  });

  it('window.onload and document.onload share one handler slot', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      document.onload = function first() {};
      window.onload = function second() {};
      JSON.stringify({ same: document.onload === window.onload });
    `);
    expect(JSON.parse(result)).toEqual({ same: true });
  });

  it('element.click() dispatches a bubbling, cancelable click event', async () => {
    dom = JSDOM.create('<html><body><div id="parent"><button id="b"></button></div></body></html>');
    const result = await dom.evaluate(`
      const btn = document.getElementById('b');
      const parent = document.getElementById('parent');
      const log = [];
      btn.addEventListener('click', (e) => log.push({ where: 'btn', bubbles: e.bubbles, cancelable: e.cancelable }));
      parent.addEventListener('click', () => log.push({ where: 'parent' }));
      btn.click();
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual([
      { where: 'btn', bubbles: true, cancelable: true },
      { where: 'parent' },
    ]);
  });

  it('element.focus() updates document.activeElement and fires focus/blur across elements', async () => {
    dom = JSDOM.create('<html><body><input id="a"><input id="b"></body></html>');
    const result = await dom.evaluate(`
      const a = document.getElementById('a');
      const b = document.getElementById('b');
      const log = [];
      a.addEventListener('focus', () => log.push('a:focus'));
      a.addEventListener('blur', () => log.push('a:blur'));
      b.addEventListener('focus', () => log.push('b:focus'));
      a.focus();
      log.push(document.activeElement.id);
      b.focus();
      log.push(document.activeElement.id);
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['a:focus', 'a', 'a:blur', 'b:focus', 'b']);
  });

  it('element.blur() clears document.activeElement back to document.body and fires blur', async () => {
    dom = JSDOM.create('<html><body><input id="a"></body></html>');
    const result = await dom.evaluate(`
      const a = document.getElementById('a');
      const log = [];
      a.addEventListener('blur', () => log.push('blur'));
      a.focus();
      a.blur();
      log.push(document.activeElement === document.body);
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['blur', true]);
  });
});
