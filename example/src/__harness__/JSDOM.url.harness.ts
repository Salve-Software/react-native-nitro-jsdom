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
      params.append('0', 'first');
      params.delete('b');
      params.sort();
      JSON.stringify({ all, afterSet, toString: params.toString(), entries: Array.from(params) });
    `);
    expect(JSON.parse(result)).toEqual({
      all: ['1', '3'],
      afterSet: ['9'],
      toString: '0=first&a=9',
      entries: [['0', 'first'], ['a', '9']],
    });
  });

  it('URL.canParse() reports parseability without throwing or constructing a URL', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        absolute: URL.canParse('https://example.com/x'),
        relativeWithBase: URL.canParse('/path', 'https://example.com'),
        invalid: URL.canParse('not a url'),
      });
    `);
    expect(JSON.parse(result)).toEqual({ absolute: true, relativeWithBase: true, invalid: false });
  });

  it('<a>/<area> href resolves relative URLs and decomposes into protocol/hostname/pathname/etc, other elements get undefined', async () => {
    dom = JSDOM.create(
      `<html><body>
        <a id="rel" href="/discount?code=SAVE10#top">10% off</a>
        <area id="area" href="page2.html">
        <div id="notLink"></div>
      </body></html>`,
      { url: 'https://shop.example.com/dir/page.html' }
    );
    const result = await dom.evaluate(`
      const a = document.getElementById('rel');
      const area = document.getElementById('area');
      const div = document.getElementById('notLink');
      JSON.stringify({
        href: a.href,
        protocol: a.protocol,
        hostname: a.hostname,
        pathname: a.pathname,
        search: a.search,
        hash: a.hash,
        origin: a.origin,
        areaHref: area.href,
        divHref: div.href,
        divProtocol: div.protocol,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      href: 'https://shop.example.com/discount?code=SAVE10#top',
      protocol: 'https:',
      hostname: 'shop.example.com',
      pathname: '/discount',
      search: '?code=SAVE10',
      hash: '#top',
      origin: 'https://shop.example.com',
      areaHref: 'https://shop.example.com/dir/page2.html',
      divHref: undefined,
      divProtocol: undefined,
    });
  });

  it('setting a.href writes the raw href attribute', async () => {
    dom = JSDOM.create('<html><body><a id="a" href="/old">old</a></body></html>', {
      url: 'https://example.com/',
    });
    const result = await dom.evaluate(`
      const a = document.getElementById('a');
      a.href = '/new';
      JSON.stringify({ attr: a.getAttribute('href'), resolved: a.href });
    `);
    expect(JSON.parse(result)).toEqual({ attr: '/new', resolved: 'https://example.com/new' });
  });
});
