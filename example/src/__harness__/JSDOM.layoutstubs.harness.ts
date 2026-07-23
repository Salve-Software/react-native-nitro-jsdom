import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM layout stubs (no rendering engine backs any of this)', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('offset*/client*/scroll* dimensions are always 0, offsetParent is null', async () => {
    dom = JSDOM.create('<html><body><div id="x">hi</div></body></html>');
    const result = await dom.evaluate(`
      const el = document.getElementById('x');
      JSON.stringify({
        offsetWidth: el.offsetWidth, offsetHeight: el.offsetHeight,
        offsetTop: el.offsetTop, offsetLeft: el.offsetLeft, offsetParent: el.offsetParent,
        clientWidth: el.clientWidth, clientHeight: el.clientHeight,
        clientTop: el.clientTop, clientLeft: el.clientLeft,
        scrollWidth: el.scrollWidth, scrollHeight: el.scrollHeight,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      offsetWidth: 0, offsetHeight: 0, offsetTop: 0, offsetLeft: 0, offsetParent: null,
      clientWidth: 0, clientHeight: 0, clientTop: 0, clientLeft: 0,
      scrollWidth: 0, scrollHeight: 0,
    });
  });

  it('scrollTop/scrollLeft round-trip a set value per element instead of hardcoding 0', async () => {
    dom = JSDOM.create('<html><body><div id="a"></div><div id="b"></div></body></html>');
    const result = await dom.evaluate(`
      const a = document.getElementById('a');
      const b = document.getElementById('b');
      const beforeSet = { top: a.scrollTop, left: a.scrollLeft };
      a.scrollTop = 50;
      a.scrollLeft = 25;
      JSON.stringify({ beforeSet, aAfter: { top: a.scrollTop, left: a.scrollLeft }, bUnaffected: { top: b.scrollTop, left: b.scrollLeft } });
    `);
    expect(JSON.parse(result)).toEqual({
      beforeSet: { top: 0, left: 0 },
      aAfter: { top: 50, left: 25 },
      bUnaffected: { top: 0, left: 0 },
    });
  });

  it('scrollIntoView/scrollTo/scrollBy/scroll are no-ops on Element and window', async () => {
    dom = JSDOM.create('<html><body><div id="x"></div></body></html>');
    const result = await dom.evaluate(`
      const el = document.getElementById('x');
      el.scrollIntoView();
      el.scrollIntoView({ behavior: 'smooth' });
      el.scrollTo(0, 100);
      el.scrollBy(10, 10);
      el.scroll(0, 0);
      scrollTo(0, 100);
      scrollBy(10, 10);
      scroll(0, 0);
      JSON.stringify({ scrollX, scrollY, pageXOffset, pageYOffset, ok: true });
    `);
    expect(JSON.parse(result)).toEqual({ scrollX: 0, scrollY: 0, pageXOffset: 0, pageYOffset: 0, ok: true });
  });

  it('document.elementFromPoint/elementsFromPoint return null/[] without throwing', async () => {
    dom = JSDOM.create('<html><body><div id="x"></div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        fromPoint: document.elementFromPoint(10, 10),
        elementsFromPoint: document.elementsFromPoint(10, 10),
      });
    `);
    expect(JSON.parse(result)).toEqual({ fromPoint: null, elementsFromPoint: [] });
  });

  it('ResizeObserver is constructible and inert — observe/unobserve/disconnect never invoke the callback', async () => {
    dom = JSDOM.create('<html><body><div id="x"></div></body></html>');
    const result = await dom.evaluate(`
      let called = false;
      const ro = new ResizeObserver(() => { called = true; });
      ro.observe(document.getElementById('x'));
      ro.unobserve(document.getElementById('x'));
      ro.disconnect();
      let caught;
      try { new ResizeObserver('not a function'); } catch (e) { caught = e.constructor.name; }
      JSON.stringify({ called, caught });
    `);
    expect(JSON.parse(result)).toEqual({ called: false, caught: 'TypeError' });
  });

  it('IntersectionObserver reflects constructor options and is inert', async () => {
    dom = JSDOM.create('<html><body><div id="root"></div><div id="target"></div></body></html>');
    const result = await dom.evaluate(`
      let called = false;
      const root = document.getElementById('root');
      const io = new IntersectionObserver(() => { called = true; }, { root, rootMargin: '10px', threshold: [0, 0.5, 1] });
      io.observe(document.getElementById('target'));
      const takeRecordsResult = io.takeRecords();
      io.disconnect();
      JSON.stringify({
        called, sameRoot: io.root === root, rootMargin: io.rootMargin, thresholds: io.thresholds,
        takeRecordsResult,
        defaultRootMargin: new IntersectionObserver(() => {}).rootMargin,
        defaultThresholds: new IntersectionObserver(() => {}).thresholds,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      called: false, sameRoot: true, rootMargin: '10px', thresholds: [0, 0.5, 1],
      takeRecordsResult: [],
      defaultRootMargin: '0px', defaultThresholds: [0],
    });
  });
});
