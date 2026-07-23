import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM navigator/matchMedia', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('navigator exposes userAgent/language/onLine', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        hasUserAgent: typeof navigator.userAgent === 'string' && navigator.userAgent.length > 0,
        language: navigator.language,
        onLine: navigator.onLine,
      });
    `);
    expect(JSON.parse(result)).toEqual({ hasUserAgent: true, language: 'en-US', onLine: true });
  });

  it('navigator.sendBeacon() posts through onFetch and returns true synchronously', async () => {
    let receivedUrl: string | undefined;
    let receivedMethod: string | undefined;
    let receivedBody: string | undefined;
    dom = JSDOM.create('<html><body></body></html>', {
      onFetch: async (url, init) => {
        receivedUrl = url;
        receivedMethod = init.method;
        receivedBody = init.body;
        return { status: 204, body: '' };
      },
    });
    const result = await dom.evaluate(`
      (async () => {
        const returned = navigator.sendBeacon('https://example.com/analytics', '{"event":"view"}');
        await new Promise((resolve) => setTimeout(resolve, 10));
        return JSON.stringify({ returned });
      })()
    `);
    expect(JSON.parse(result)).toEqual({ returned: true });
    expect(receivedUrl).toBe('https://example.com/analytics');
    expect(receivedMethod).toBe('POST');
    expect(receivedBody).toBe('{"event":"view"}');
  });

  it('matchMedia returns a MediaQueryList-like stub that never matches', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const mql = matchMedia('(min-width: 600px)');
      let changeFired = false;
      mql.addEventListener('change', () => { changeFired = true; });
      JSON.stringify({ media: mql.media, matches: mql.matches, changeFired });
    `);
    expect(JSON.parse(result)).toEqual({ media: '(min-width: 600px)', matches: false, changeFired: false });
  });

  it('history.pushState/replaceState track state and length without firing popstate', async () => {
    dom = JSDOM.create('<html><body></body></html>', { url: 'https://example.com/' });
    const result = await dom.evaluate(`
      let popstateFired = false;
      addEventListener('popstate', () => { popstateFired = true; });
      const initialState = history.state;
      const initialLength = history.length;
      history.pushState({ page: 1 }, '', '/page-1');
      history.pushState({ page: 2 }, '', '/page-2');
      history.replaceState({ page: 2, replaced: true }, '', '/page-2b');
      JSON.stringify({
        initialState, initialLength,
        state: history.state, length: history.length,
        href: location.href, popstateFired,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      initialState: null, initialLength: 1,
      state: { page: 2, replaced: true }, length: 3,
      href: 'https://example.com/page-2b', popstateFired: false,
    });
  });

  it('history.back()/forward()/go() navigate the entry stack and fire popstate with the entry state', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const popstates = [];
      addEventListener('popstate', (e) => { popstates.push(e.state); });
      history.pushState('a', '');
      history.pushState('b', '');
      history.back();
      const afterBack = history.state;
      history.forward();
      const afterForward = history.state;
      history.go(-2);
      const afterGoNegative = history.state;
      history.go(-99);
      const afterOutOfRangeGo = history.state;
      JSON.stringify({ afterBack, afterForward, afterGoNegative, afterOutOfRangeGo, popstates });
    `);
    expect(JSON.parse(result)).toEqual({
      afterBack: 'a', afterForward: 'b', afterGoNegative: null, afterOutOfRangeGo: null,
      popstates: ['a', 'b', null],
    });
  });

  it('history.pushState/replaceState clone state, so mutating the caller object after the call does not leak in', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const pushed = { count: 1 };
      history.pushState(pushed, '');
      pushed.count = 999;
      const afterPushMutation = history.state;
      const replaced = { count: 2 };
      history.replaceState(replaced, '');
      replaced.count = 888;
      const afterReplaceMutation = history.state;
      JSON.stringify({ afterPushMutation, afterReplaceMutation, sameReference: history.state === replaced });
    `);
    expect(JSON.parse(result)).toEqual({
      afterPushMutation: { count: 1 }, afterReplaceMutation: { count: 2 }, sameReference: false,
    });
  });

  it('history.go() truncates a fractional/NaN delta instead of landing on a non-integer index', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      history.pushState('a', '');
      history.pushState('b', '');
      history.go(-0.9);
      const afterFractionalNoop = history.state;
      history.go(-1.5);
      const afterTruncatedToOne = history.state;
      history.go(NaN);
      const afterNaN = history.state;
      JSON.stringify({ afterFractionalNoop, afterTruncatedToOne, afterNaN });
    `);
    expect(JSON.parse(result)).toEqual({
      afterFractionalNoop: 'b', afterTruncatedToOne: 'a', afterNaN: 'a',
    });
  });

  it('getSelection() returns a stub Selection that never has a range', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const sel = getSelection();
      const sameInstance = getSelection() === sel;
      let caught;
      try { sel.getRangeAt(0); } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
      JSON.stringify({
        rangeCount: sel.rangeCount, isCollapsed: sel.isCollapsed, toString: sel.toString(),
        sameInstance, caught,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      rangeCount: 0, isCollapsed: true, toString: '', sameInstance: true,
      caught: { name: 'IndexSizeError', isDOMException: true },
    });
  });
});
