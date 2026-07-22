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
});
