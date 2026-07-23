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
      events: ['listener', 'onabort'],
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

  it('Request constructor defaults method to GET and wraps headers in a Headers instance', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const req = new Request('https://example.com/a', { headers: { 'X-Test': '1' } });
      JSON.stringify({
        url: req.url,
        method: req.method,
        header: req.headers.get('x-test'),
        bodyUsed: req.bodyUsed,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      url: 'https://example.com/a',
      method: 'GET',
      header: '1',
      bodyUsed: false,
    });
  });

  it('fetch(new Request(...)) forwards the request method/headers/body to onFetch', async () => {
    dom = JSDOM.create('<html><body></body></html>', {
      onFetch: async (url, init) => {
        expect(url).toBe('https://example.com/ping');
        expect(init.method).toBe('POST');
        expect(init.body).toBe('{"ping":true}');
        return { status: 200, body: 'ok' };
      },
    });
    const result = await dom.evaluate(`
      (async () => {
        const req = new Request('https://example.com/ping', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ping: true }),
        });
        const res = await fetch(req);
        return res.text();
      })()
    `);
    expect(result).toBe('ok');
  });

  it('new Request(existingRequest, init) inherits url/headers and overrides only what init sets', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const base = new Request('https://example.com/b', { method: 'POST', headers: { 'X-Test': '1' } });
      const derived = new Request(base, { method: 'PUT' });
      JSON.stringify({
        url: derived.url,
        method: derived.method,
        header: derived.headers.get('x-test'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      url: 'https://example.com/b',
      method: 'PUT',
      header: '1',
    });
  });
});
