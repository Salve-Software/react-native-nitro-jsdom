import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM structuredClone', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('deep-clones nested objects/arrays, producing a value-equal but reference-distinct copy', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const original = { a: 1, nested: { b: [1, 2, { c: 3 }] } };
      const clone = structuredClone(original);
      JSON.stringify({
        equalButNotSame: JSON.stringify(clone) === JSON.stringify(original) && clone !== original,
        nestedObjectIsCopy: clone.nested !== original.nested,
        nestedArrayIsCopy: clone.nested.b !== original.nested.b,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      equalButNotSame: true,
      nestedObjectIsCopy: true,
      nestedArrayIsCopy: true,
    });
  });

  it('clones Date, RegExp, Map, and Set', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const date = new Date(1700000000000);
      const regexp = /abc/gi;
      const map = new Map([['x', 1], ['y', 2]]);
      const set = new Set([1, 2, 3]);
      const cloned = structuredClone({ date, regexp, map, set });
      JSON.stringify({
        dateOk: cloned.date instanceof Date && cloned.date.getTime() === date.getTime() && cloned.date !== date,
        regexpOk: cloned.regexp instanceof RegExp && cloned.regexp.source === 'abc' && cloned.regexp.flags === 'gi',
        mapOk: cloned.map instanceof Map && cloned.map.get('x') === 1 && cloned.map !== map,
        setOk: cloned.set instanceof Set && cloned.set.has(3) && cloned.set !== set,
      });
    `);
    expect(JSON.parse(result)).toEqual({ dateOk: true, regexpOk: true, mapOk: true, setOk: true });
  });

  it('preserves circular references instead of infinite-looping', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const original = { name: 'root' };
      original.self = original;
      const clone = structuredClone(original);
      JSON.stringify({ selfReferential: clone.self === clone, notSameAsOriginal: clone !== original });
    `);
    expect(JSON.parse(result)).toEqual({ selfReferential: true, notSameAsOriginal: true });
  });

  it('throws a DataCloneError DOMException for functions', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let caught;
      try { structuredClone({ fn: function () {} }); } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'DataCloneError', isDOMException: true });
  });
});
