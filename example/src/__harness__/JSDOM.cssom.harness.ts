import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM CSSOM (document.styleSheets / CSSStyleRule)', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('parses a <style> element into cssRules with selectorText and a live style object', async () => {
    dom = JSDOM.create(`
      <html><head>
        <style>.foo { color: red; font-size: 12px; } #bar { display: none; }</style>
      </head><body></body></html>
    `);
    const result = await dom.evaluate(`
      const sheet = document.styleSheets[0];
      JSON.stringify({
        length: sheet.cssRules.length,
        first: { selectorText: sheet.cssRules[0].selectorText, color: sheet.cssRules[0].style.color, fontSize: sheet.cssRules[0].style.fontSize },
        second: { selectorText: sheet.cssRules[1].selectorText, display: sheet.cssRules[1].style.display },
        type: sheet.cssRules[0].type,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      length: 2,
      first: { selectorText: '.foo', color: 'red', fontSize: '12px' },
      second: { selectorText: '#bar', display: 'none' },
      type: 1,
    });
  });

  it('mutating rule.style updates cssText, and document.styleSheets caches across accesses', async () => {
    dom = JSDOM.create('<html><head><style>.foo { color: red; }</style></head><body></body></html>');
    const result = await dom.evaluate(`
      document.styleSheets[0].cssRules[0].style.color = 'blue';
      const cssTextAfter = document.styleSheets[0].cssRules[0].cssText;
      const sameSheet = document.styleSheets[0] === document.styleSheets[0];
      JSON.stringify({ cssTextAfter, sameSheet });
    `);
    expect(JSON.parse(result)).toEqual({ cssTextAfter: '.foo { color: blue; }', sameSheet: true });
  });

  it('insertRule/deleteRule mutate cssRules and throw IndexSizeError DOMException out of range', async () => {
    dom = JSDOM.create('<html><head><style>.a { color: red; }</style></head><body></body></html>');
    const result = await dom.evaluate(`
      const sheet = document.styleSheets[0];
      const insertedIndex = sheet.insertRule('.b { color: green; }', 0);
      const afterInsert = Array.from(sheet.cssRules).map((r) => r.selectorText);
      sheet.deleteRule(1);
      const afterDelete = Array.from(sheet.cssRules).map((r) => r.selectorText);
      let caught;
      try { sheet.deleteRule(99); } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
      JSON.stringify({ insertedIndex, afterInsert, afterDelete, caught });
    `);
    expect(JSON.parse(result)).toEqual({
      insertedIndex: 0,
      afterInsert: ['.b', '.a'],
      afterDelete: ['.b'],
      caught: { name: 'IndexSizeError', isDOMException: true },
    });
  });

  it('collects multiple <style> elements in document order', async () => {
    dom = JSDOM.create(`
      <html><head><style>.a { color: red; }</style></head>
      <body><style>.b { color: blue; }</style></body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.styleSheets).map((s) => s.cssRules[0].selectorText));
    `);
    expect(JSON.parse(result)).toEqual(['.a', '.b']);
  });

  it('at-rules like @media are captured as opaque stub rules without crashing', async () => {
    dom = JSDOM.create(`
      <html><head>
        <style>@media (min-width: 600px) { .a { color: red; } } .b { color: blue; }</style>
      </head><body></body></html>
    `);
    const result = await dom.evaluate(`
      const sheet = document.styleSheets[0];
      JSON.stringify({
        length: sheet.cssRules.length,
        firstType: sheet.cssRules[0].type,
        secondSelector: sheet.cssRules[1].selectorText,
      });
    `);
    expect(JSON.parse(result)).toEqual({ length: 2, firstType: 0, secondSelector: '.b' });
  });

  it('getComputedStyle reads inline style, camelCase and kebab-case alike', async () => {
    dom = JSDOM.create('<html><body><div id="x" style="color: red; font-size: 12px;"></div></body></html>');
    const result = await dom.evaluate(`
      const cs = getComputedStyle(document.getElementById('x'));
      JSON.stringify({
        camel: cs.fontSize,
        kebab: cs.getPropertyValue('font-size'),
        color: cs.color,
        missing: cs.backgroundColor,
      });
    `);
    expect(JSON.parse(result)).toEqual({ camel: '12px', kebab: '12px', color: 'red', missing: '' });
  });

  it('getComputedStyle falls back to tag-default display when not set inline', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="block"></div>
        <span id="inline"></span>
        <img id="imgtag" />
        <div id="hiddenAttr" hidden></div>
        <div id="hiddenInline" style="display: none;"></div>
      </body></html>
    `);
    const result = await dom.evaluate(`
      function displayOf(id) { return getComputedStyle(document.getElementById(id)).display; }
      JSON.stringify({
        block: displayOf('block'),
        inline: displayOf('inline'),
        img: displayOf('imgtag'),
        hiddenAttr: displayOf('hiddenAttr'),
        hiddenInline: displayOf('hiddenInline'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      block: 'block', inline: 'inline', img: 'inline-block', hiddenAttr: 'none', hiddenInline: 'none',
    });
  });

  it('getComputedStyle defaults visibility/opacity and throws TypeError for a non-Element argument', async () => {
    dom = JSDOM.create('<html><body><div id="x"></div></body></html>');
    const result = await dom.evaluate(`
      const cs = getComputedStyle(document.getElementById('x'));
      let caught;
      try { getComputedStyle({}); } catch (e) { caught = e.constructor.name; }
      JSON.stringify({ visibility: cs.visibility, opacity: cs.opacity, caught });
    `);
    expect(JSON.parse(result)).toEqual({ visibility: 'visible', opacity: '1', caught: 'TypeError' });
  });

  it('CSS.escape() escapes special characters and a leading digit/hyphen-digit per the CSSOM spec', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        idWithColon: CSS.escape('a:b'),
        leadingDigit: CSS.escape('1a'),
        leadingHyphenDigit: CSS.escape('-1a'),
        lonelyHyphen: CSS.escape('-'),
        plain: CSS.escape('plain-id_1'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      idWithColon: 'a\\:b',
      leadingDigit: '\\31 a',
      leadingHyphenDigit: '-\\31 a',
      lonelyHyphen: '\\-',
      plain: 'plain-id_1',
    });
  });

  it('CSS.escape() output round-trips through querySelector on a dynamic id', async () => {
    dom = JSDOM.create('<html><body><div id="weird:id.with.dots"></div></body></html>');
    const result = await dom.evaluate(`
      const dynamicId = 'weird:id.with.dots';
      const el = document.querySelector('#' + CSS.escape(dynamicId));
      String(el && el.id);
    `);
    expect(result).toBe('weird:id.with.dots');
  });

  it('CSS.supports() reports true for a plausible property/value pair and false for empty input', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        withValue: CSS.supports('display', 'flex'),
        emptyValue: CSS.supports('display', ''),
        conditionText: CSS.supports('display: flex'),
        emptyCondition: CSS.supports(''),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      withValue: true,
      emptyValue: false,
      conditionText: true,
      emptyCondition: false,
    });
  });
});
