import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM DOMException', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('is constructible directly with a message and name', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const e = new DOMException('nope', 'NotFoundError');
      JSON.stringify({
        message: e.message,
        name: e.name,
        code: e.code,
        isError: e instanceof Error,
        toString: e.toString(),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      message: 'nope',
      name: 'NotFoundError',
      code: 8,
      isError: true,
      toString: 'NotFoundError: nope',
    });
  });

  it('exposes legacy numeric constants on the constructor and instances', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const e = new DOMException('x', 'SyntaxError');
      JSON.stringify({
        staticConst: DOMException.SYNTAX_ERR,
        instanceConst: e.SYNTAX_ERR,
      });
    `);
    expect(JSON.parse(result)).toEqual({ staticConst: 12, instanceConst: 12 });
  });

  it('insertBefore throws a real NotFoundError DOMException for a non-child reference node', async () => {
    dom = JSDOM.create('<html><body><div id="a"></div><div id="b"></div></body></html>');
    const result = await dom.evaluate(`
      const a = document.getElementById('a');
      const b = document.getElementById('b');
      const span = document.createElement('span');
      let caught;
      try {
        a.insertBefore(span, b);
      } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'NotFoundError', isDOMException: true });
  });

  it('classList.add throws a real SyntaxError DOMException for an empty token', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      let caught;
      try {
        div.classList.add('');
      } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'SyntaxError', isDOMException: true });
  });

  it('classList.add throws a real InvalidCharacterError DOMException for a token with whitespace', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      let caught;
      try {
        div.classList.add('foo bar');
      } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'InvalidCharacterError', isDOMException: true });
  });
});
