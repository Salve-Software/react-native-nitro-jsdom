import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM fetch/XHR/AbortController', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
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
});
