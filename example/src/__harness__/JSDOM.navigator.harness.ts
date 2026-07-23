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
      state: { page: 2, replaced: true }, length: 2,
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
