import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM URL/URLSearchParams', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('URL parses and serializes components, resolving relative URLs against a base', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const url = new URL('/path?a=1#hash', 'https://user:pass@example.com:8080');
      JSON.stringify({
        href: url.href,
        protocol: url.protocol,
        hostname: url.hostname,
        port: url.port,
        pathname: url.pathname,
        search: url.search,
        hash: url.hash,
        origin: url.origin,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      href: 'https://user:pass@example.com:8080/path?a=1#hash',
      protocol: 'https:',
      hostname: 'example.com',
      port: '8080',
      pathname: '/path',
      search: '?a=1',
      hash: '#hash',
      origin: 'https://example.com:8080',
    });
  });

  it('URL.searchParams stays in sync with url.search in both directions', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const url = new URL('https://example.com/?a=1');
      url.searchParams.append('b', '2');
      const afterAppend = url.search;
      url.search = '?c=3';
      const afterSearchSet = url.searchParams.get('c');
      JSON.stringify({ afterAppend, afterSearchSet, hasA: url.searchParams.has('a') });
    `);
    expect(JSON.parse(result)).toEqual({ afterAppend: '?a=1&b=2', afterSearchSet: '3', hasA: false });
  });

  it('URLSearchParams supports append/get/getAll/delete/set/sort/iteration', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const params = new URLSearchParams('b=2&a=1&a=3');
      const all = params.getAll('a');
      params.set('a', '9');
      const afterSet = params.getAll('a');
      params.delete('b');
      params.sort();
      JSON.stringify({ all, afterSet, toString: params.toString(), entries: Array.from(params) });
    `);
    expect(JSON.parse(result)).toEqual({
      all: ['1', '3'],
      afterSet: ['9'],
      toString: 'a=9',
      entries: [['a', '9']],
    });
  });
});
