import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const locationSection = async (): Promise<ISection> => {
  const html = `<html><body></body></html>`;
  const results = [];

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`String(window.location.href)`);
    results.push({ label: 'default url — expect: about:blank', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`String(window.location.origin)`);
    results.push({ label: 'about:blank origin — expect: null', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { url: 'https://example.com:8080/path/to/page?query=1#hash' });
    const res = await dom.evaluate(`
      JSON.stringify({
        protocol: location.protocol,
        hostname: location.hostname,
        port: location.port,
        host: location.host,
        pathname: location.pathname,
        search: location.search,
        hash: location.hash,
        origin: location.origin,
      })
    `);
    results.push({
      label: 'full URL parsed components — expect: {"protocol":"https:","hostname":"example.com","port":"8080","host":"example.com:8080","pathname":"/path/to/page","search":"?query=1","hash":"#hash","origin":"https://example.com:8080"}',
      value: res,
    });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { url: 'https://example.com/' });
    const res = await dom.evaluate(`
      location.href = 'https://other.com/new';
      JSON.stringify({ href: location.href, hostname: location.hostname });
    `);
    results.push({ label: 'set location.href re-parses — expect: {"href":"https://other.com/new","hostname":"other.com"}', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { url: 'https://example.com/' });
    const res = await dom.evaluate(`
      location.assign('https://assigned.com/x');
      location.href
    `);
    results.push({ label: 'location.assign() updates href — expect: https://assigned.com/x', value: res });
    dom.dispose();
  }

  return { title: 'Location', results };
};
