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
});
