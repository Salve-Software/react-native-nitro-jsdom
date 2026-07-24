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

  it('console.group/groupEnd/trace/table forward through onConsole as log level', async () => {
    const captured: string[] = [];
    dom = JSDOM.create('<html><body></body></html>', {
      onConsole: (level, args) => captured.push(`${level}:${args.join(',')}`),
    });
    await dom.evaluate(`
      console.group('widget init');
      console.trace('entering render');
      console.table({ a: 1 });
      console.groupEnd();
    `);
    expect(captured).toEqual([
      'log:widget init',
      'log:Trace:,entering render',
      'log:{"a":1}',
    ]);
  });

  it('console.assert only logs (via error) when the condition is falsy', async () => {
    const captured: string[] = [];
    dom = JSDOM.create('<html><body></body></html>', {
      onConsole: (level, args) => captured.push(`${level}:${args.join(',')}`),
    });
    await dom.evaluate(`
      console.assert(true, 'should not appear');
      console.assert(false, 'discount code missing');
    `);
    expect(captured).toEqual(['error:Assertion failed:,discount code missing']);
  });

  it('console.count/countReset track per-label counters', async () => {
    const captured: string[] = [];
    dom = JSDOM.create('<html><body></body></html>', {
      onConsole: (level, args) => captured.push(`${level}:${args.join(',')}`),
    });
    await dom.evaluate(`
      console.count('widget');
      console.count('widget');
      console.countReset('widget');
      console.count('widget');
    `);
    expect(captured).toEqual(['log:widget: 1', 'log:widget: 2', 'log:widget: 1']);
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

  it('document.visibilityState mirrors document.hidden', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const defaultResult = await dom.evaluate('document.visibilityState');
    expect(defaultResult).toBe('hidden');
    dom.dispose();

    dom = JSDOM.create('<html><body></body></html>', { pretendToBeVisual: true });
    const visibleResult = await dom.evaluate('document.visibilityState');
    expect(visibleResult).toBe('visible');
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

  it('fires DOMContentLoaded then load, once each, on document and window listeners registered by inline scripts', async () => {
    dom = JSDOM.create(`
      <html><body>
        <script>
          window.__events = [];
          document.addEventListener('DOMContentLoaded', function() { window.__events.push('doc:DOMContentLoaded'); });
          window.addEventListener('DOMContentLoaded', function() { window.__events.push('win:DOMContentLoaded'); });
          window.addEventListener('load', function() { window.__events.push('win:load'); });
        </script>
      </body></html>
    `);
    const result = await dom.evaluate('JSON.stringify(window.__events)');
    expect(JSON.parse(result)).toEqual(['doc:DOMContentLoaded', 'win:DOMContentLoaded', 'win:load']);
  });

  it('document.readyState is "complete" once evaluate() sees the document', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate('document.readyState');
    expect(result).toBe('complete');
  });

  it('document.readyState transitions loading -> interactive -> complete around DOMContentLoaded/load', async () => {
    dom = JSDOM.create(`
      <html><body>
        <script>
          window.__states = [];
          window.__states.push(document.readyState);
          document.addEventListener('DOMContentLoaded', function() { window.__states.push(document.readyState); });
          window.addEventListener('load', function() { window.__states.push(document.readyState); });
        </script>
      </body></html>
    `);
    const result = await dom.evaluate('JSON.stringify(window.__states)');
    expect(JSON.parse(result)).toEqual(['loading', 'interactive', 'complete']);
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
