import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM lifecycle', () => {
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
});
