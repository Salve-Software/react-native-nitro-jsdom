import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM <template>', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('parsed <template> markup lives in .content, not as light-DOM children', async () => {
    dom = JSDOM.create('<html><body><template id="t"><p>hi</p></template></body></html>');
    const result = await dom.evaluate(`
      const tpl = document.getElementById('t');
      JSON.stringify({
        lightChildNodes: tpl.childNodes.length,
        contentChildNodes: tpl.content.childNodes.length,
        contentFirstText: tpl.content.firstChild.textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      lightChildNodes: 0,
      contentChildNodes: 1,
      contentFirstText: 'hi',
    });
  });

  it('document.createElement("template") also gets a populated .content fragment', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const tpl = document.createElement('template');
      const span = document.createElement('span');
      span.textContent = 'made';
      tpl.content.appendChild(span);
      JSON.stringify({
        contentChildNodes: tpl.content.childNodes.length,
        text: tpl.content.firstChild.textContent,
        lightChildNodes: tpl.childNodes.length,
      });
    `);
    expect(JSON.parse(result)).toEqual({ contentChildNodes: 1, text: 'made', lightChildNodes: 0 });
  });

  it('setting template.innerHTML populates .content instead of light-DOM children', async () => {
    dom = JSDOM.create('<html><body><template id="t"></template></body></html>');
    const result = await dom.evaluate(`
      const tpl = document.getElementById('t');
      tpl.innerHTML = '<span>new</span>';
      JSON.stringify({
        lightChildNodes: tpl.childNodes.length,
        contentFirstTag: tpl.content.firstChild.tagName,
        contentText: tpl.content.firstChild.textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      lightChildNodes: 0,
      contentFirstTag: 'SPAN',
      contentText: 'new',
    });
  });

  it('.content getter returns undefined for non-<template> elements', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`String(document.getElementById('d').content)`);
    expect(result).toBe('undefined');
  });
});
