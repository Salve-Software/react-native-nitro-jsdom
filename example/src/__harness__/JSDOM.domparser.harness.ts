import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM DOMParser / createHTMLDocument', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('DOMParser.parseFromString() parses HTML into a queryable, separate document', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const parsed = new DOMParser().parseFromString(
        '<html><body><p id="x">hello</p></body></html>', 'text/html'
      );
      JSON.stringify({
        text: parsed.getElementById('x').textContent,
        tag: parsed.documentElement.tagName,
        bodyTag: parsed.body.tagName,
        headTag: parsed.head.tagName,
      });
    `);
    expect(JSON.parse(result)).toEqual({ text: 'hello', tag: 'HTML', bodyTag: 'BODY', headTag: 'HEAD' });
  });

  it('parsed documents are instanceof Document but distinct from the sandbox document', async () => {
    dom = JSDOM.create('<html><body><p id="only-in-main">main</p></body></html>');
    const result = await dom.evaluate(`
      const parsed = new DOMParser().parseFromString('<html><body><p id="x">x</p></body></html>', 'text/html');
      JSON.stringify({
        isDocument: parsed instanceof Document,
        notSameAsMain: parsed !== document,
        mainCannotSeeParsedNode: document.getElementById('x'),
        parsedCannotSeeMainNode: parsed.getElementById('only-in-main'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      isDocument: true,
      notSameAsMain: true,
      mainCannotSeeParsedNode: null,
      parsedCannotSeeMainNode: null,
    });
  });

  it('parseFromString() throws TypeError for an unsupported MIME type', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      try {
        new DOMParser().parseFromString('<p>x</p>', 'text/plain');
        JSON.stringify({ threw: false });
      } catch (e) {
        JSON.stringify({ threw: true, name: e.constructor.name });
      }
    `);
    expect(JSON.parse(result)).toEqual({ threw: true, name: 'TypeError' });
  });

  it('document.implementation.createHTMLDocument() creates a minimal document with the given title', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const doc = document.implementation.createHTMLDocument('My Title');
      JSON.stringify({
        title: doc.title,
        headTitleText: doc.head.querySelector('title').textContent,
        hasBody: doc.body !== null,
      });
    `);
    expect(JSON.parse(result)).toEqual({ title: 'My Title', headTitleText: 'My Title', hasBody: true });
  });

  it('createHTMLDocument() defaults to an empty title and HTML-escapes it', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const untitled = document.implementation.createHTMLDocument();
      const escaped = document.implementation.createHTMLDocument('<b>&"</b>');
      JSON.stringify({
        untitledTitle: untitled.title,
        escapedTitleText: escaped.title,
      });
    `);
    expect(JSON.parse(result)).toEqual({ untitledTitle: '', escapedTitleText: '<b>&"</b>' });
  });

  it('title setter on a created document creates/updates the <title> element', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const doc = document.implementation.createHTMLDocument('Old');
      doc.title = 'New';
      JSON.stringify({ title: doc.title, headTitleText: doc.head.querySelector('title').textContent });
    `);
    expect(JSON.parse(result)).toEqual({ title: 'New', headTitleText: 'New' });
  });

  it('elements from a parsed document fully support mutation: createElement/appendChild/setAttribute/textContent/innerHTML', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const parsed = new DOMParser().parseFromString('<html><body><div id="root"></div></body></html>', 'text/html');
      const root = parsed.getElementById('root');

      const child = parsed.createElement('span');
      child.setAttribute('class', 'greeting');
      child.textContent = 'hi';
      root.appendChild(child);

      root.innerHTML += '<p>via innerHTML</p>';

      JSON.stringify({
        childText: root.querySelector('.greeting').textContent,
        childCount: root.children.length,
        lastTag: root.lastElementChild.tagName,
        lastText: root.lastElementChild.textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      childText: 'hi',
      childCount: 2,
      lastTag: 'P',
      lastText: 'via innerHTML',
    });
  });

  it('getElementsByClassName/getElementsByTagName on a parsed document return static arrays', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const parsed = new DOMParser().parseFromString(
        '<html><body><div class="item">a</div><div class="item">b</div><span>c</span></body></html>', 'text/html'
      );
      JSON.stringify({
        byClass: parsed.getElementsByClassName('item').length,
        byTag: parsed.getElementsByTagName('span').length,
        isArray: Array.isArray(parsed.getElementsByClassName('item')),
      });
    `);
    expect(JSON.parse(result)).toEqual({ byClass: 2, byTag: 1, isArray: true });
  });

  it('a parsed document with a doctype exposes it, and one without exposes null', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const withDoctype = new DOMParser().parseFromString(
        '<!doctype html><html><body></body></html>', 'text/html'
      );
      const withoutDoctype = new DOMParser().parseFromString('<html><body></body></html>', 'text/html');
      JSON.stringify({
        name: withDoctype.doctype ? withDoctype.doctype.name : null,
        none: withoutDoctype.doctype,
      });
    `);
    expect(JSON.parse(result)).toEqual({ name: 'html', none: null });
  });
});
