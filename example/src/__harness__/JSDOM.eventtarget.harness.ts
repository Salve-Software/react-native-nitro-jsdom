import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM standalone EventTarget', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('dispatchEvent() invokes registered listeners with target/currentTarget set, and returns !defaultPrevented', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      let received = null;
      target.addEventListener('ping', function(e) {
        received = { type: e.type, target: e.target === target, currentTarget: e.currentTarget === target, thisIsTarget: this === target };
      });
      const notCancelled = target.dispatchEvent(new Event('ping'));

      target.addEventListener('cancelme', (e) => { e.preventDefault(); });
      const cancelled = target.dispatchEvent(new Event('cancelme', { cancelable: true }));

      JSON.stringify({ received, notCancelled, cancelled });
    `);
    expect(JSON.parse(result)).toEqual({
      received: { type: 'ping', target: true, currentTarget: true, thisIsTarget: true },
      notCancelled: true,
      cancelled: false,
    });
  });

  it('removeEventListener() removes a listener, addEventListener() ignores an exact duplicate', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      let count = 0;
      function handler() { count++; }
      target.addEventListener('x', handler);
      target.addEventListener('x', handler);
      target.dispatchEvent(new Event('x'));
      target.removeEventListener('x', handler);
      target.dispatchEvent(new Event('x'));
      JSON.stringify({ count });
    `);
    expect(JSON.parse(result)).toEqual({ count: 1 });
  });

  it('an { once: true } listener fires exactly once', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      let count = 0;
      target.addEventListener('x', () => { count++; }, { once: true });
      target.dispatchEvent(new Event('x'));
      target.dispatchEvent(new Event('x'));
      JSON.stringify({ count });
    `);
    expect(JSON.parse(result)).toEqual({ count: 1 });
  });

  it('stopImmediatePropagation() during dispatch stops later listeners for the same event', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      const order = [];
      target.addEventListener('x', (e) => { order.push('first'); e.stopImmediatePropagation(); });
      target.addEventListener('x', () => { order.push('second'); });
      target.dispatchEvent(new Event('x'));
      JSON.stringify({ order });
    `);
    expect(JSON.parse(result)).toEqual({ order: ['first'] });
  });

  it('a listener object with handleEvent() is invoked instead of a plain function', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      let received = null;
      target.addEventListener('x', { handleEvent: function(e) { received = e.type; } });
      target.dispatchEvent(new Event('x'));
      JSON.stringify({ received });
    `);
    expect(JSON.parse(result)).toEqual({ received: 'x' });
  });

  it('dispatching an event with no listeners for its type is a no-op that still returns true', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      JSON.stringify({ returned: target.dispatchEvent(new Event('nobody-listening')) });
    `);
    expect(JSON.parse(result)).toEqual({ returned: true });
  });

  it('a custom class can extend EventTarget for its own pub-sub', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      class Emitter extends EventTarget {
        ping() { this.dispatchEvent(new CustomEvent('ping', { detail: 42 })); }
      }
      const emitter = new Emitter();
      let detail = null;
      emitter.addEventListener('ping', (e) => { detail = e.detail; });
      emitter.ping();
      JSON.stringify({ detail, isEventTarget: emitter instanceof EventTarget });
    `);
    expect(JSON.parse(result)).toEqual({ detail: 42, isEventTarget: true });
  });

  it('event types that collide with inherited Object properties (constructor, toString, __proto__) work normally', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      const received = [];
      ['constructor', 'toString', '__proto__', 'hasOwnProperty'].forEach((type) => {
        target.addEventListener(type, () => { received.push(type); });
      });
      ['constructor', 'toString', '__proto__', 'hasOwnProperty'].forEach((type) => {
        target.dispatchEvent(new Event(type));
      });
      JSON.stringify({ received });
    `);
    expect(JSON.parse(result)).toEqual({ received: ['constructor', 'toString', '__proto__', 'hasOwnProperty'] });
  });

  it('a listener removed by an earlier listener during the same dispatch does not fire', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      const order = [];
      function third() { order.push('third'); }
      target.addEventListener('x', () => { order.push('first'); target.removeEventListener('x', third); });
      target.addEventListener('x', () => { order.push('second'); });
      target.addEventListener('x', third);
      target.dispatchEvent(new Event('x'));
      JSON.stringify({ order });
    `);
    expect(JSON.parse(result)).toEqual({ order: ['first', 'second'] });
  });

  it('currentTarget resets to null even when a listener throws, and the exception propagates out of dispatchEvent', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const target = new EventTarget();
      target.addEventListener('x', () => { throw new Error('boom'); });
      const event = new Event('x');
      let caughtMessage = null;
      try { target.dispatchEvent(event); } catch (e) { caughtMessage = e.message; }
      JSON.stringify({ caughtMessage, currentTargetAfter: event.currentTarget });
    `);
    expect(JSON.parse(result)).toEqual({ caughtMessage: 'boom', currentTargetAfter: null });
  });
});
