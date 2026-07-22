import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM DOM mutation', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('cloneNode() shallow-clones by default and deep-clones with deep=true', async () => {
    dom = JSDOM.create('<html><body><div id="d" class="x"><span>hi</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const shallow = div.cloneNode();
      const deep = div.cloneNode(true);
      JSON.stringify({
        shallowClass: shallow.className,
        shallowChildCount: shallow.childNodes.length,
        deepChildCount: deep.childNodes.length,
        deepFirstChildTag: deep.firstChild.tagName,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      shallowClass: 'x',
      shallowChildCount: 0,
      deepChildCount: 1,
      deepFirstChildTag: 'SPAN',
    });
  });

  it('replaceChild() swaps a child and returns the removed node', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span id="old">old</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const oldEl = document.getElementById('old');
      const newEl = document.createElement('p');
      newEl.textContent = 'new';
      const returned = div.replaceChild(newEl, oldEl);
      JSON.stringify({ returnedIsOld: returned === oldEl, innerHTML: div.innerHTML });
    `);
    expect(JSON.parse(result)).toEqual({ returnedIsOld: true, innerHTML: '<p>new</p>' });
  });

  it('before()/after()/replaceWith() manipulate siblings, accepting nodes and strings', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span id="s">mid</span></div></body></html>');
    const result = await dom.evaluate(`
      const span = document.getElementById('s');
      span.before('before-text');
      span.after('after-text');
      document.getElementById('d').innerHTML;
    `);
    expect(result).toBe('before-text<span id="s">mid</span>after-text');

    const result2 = await dom.evaluate(`
      document.getElementById('s').replaceWith('replaced');
      document.getElementById('d').innerHTML;
    `);
    expect(result2).toBe('before-textreplacedafter-text');
  });

  it('append()/prepend() add children in argument order, accepting nodes and strings', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const b = document.createElement('b');
      b.textContent = 'b';
      div.append('a', b, 'c');
      div.prepend('z');
      div.innerHTML;
    `);
    expect(result).toBe('za<b>b</b>c');
  });

  it('insertAdjacentHTML() inserts parsed HTML at the four standard positions', async () => {
    dom = JSDOM.create('<html><body><div id="d"><span>mid</span></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      div.insertAdjacentHTML('afterbegin', '<i>ab</i>');
      div.insertAdjacentHTML('beforeend', '<i>be</i>');
      div.insertAdjacentHTML('beforebegin', '<i>bb</i>');
      div.insertAdjacentHTML('afterend', '<i>ae</i>');
      document.body.innerHTML;
    `);
    expect(result).toBe(
      '<i>bb</i><div id="d"><i>ab</i><span>mid</span><i>be</i></div><i>ae</i>'
    );
  });

  it('document.createComment()/createDocumentFragment() create nodes usable with appendChild', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const comment = document.createComment('hi');
      div.appendChild(comment);

      const frag = document.createDocumentFragment();
      const a = document.createElement('a');
      a.textContent = 'link';
      frag.appendChild(a);
      div.appendChild(frag);

      JSON.stringify({
        commentNodeType: comment.nodeType,
        commentNodeValue: comment.nodeValue,
        innerHTML: div.innerHTML,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      commentNodeType: 8,
      commentNodeValue: 'hi',
      innerHTML: '<!--hi--><a>link</a>',
    });
  });
});
