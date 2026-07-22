import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM timers and runtime globals', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
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
