import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Bridges the sandbox's fetch() to React Native's real fetch().
const onFetch = async (
  url: string,
  init: { method: string; headers: Record<string, string>; body?: string },
) => {
  const res = await fetch(url, init);
  const headers: Record<string, string> = {};
  res.headers.forEach((value, key) => { headers[key] = value; });
  return {
    status: res.status,
    statusText: res.statusText,
    headers,
    body: await res.text(),
  };
};

export const fetchSection = async (): Promise<ISection> => {
  const html = `<html><body></body></html>`;
  const results = [];

  {
    const dom = JSDOM.create(html);
    let rejected = false;
    try { await dom.evaluate(`fetch('https://example.com')`); }
    catch { rejected = true; }
    results.push({ label: 'fetch without onFetch — rejects — expect: true', value: String(rejected) });
    dom.dispose();
  }

  results.push({
    label: 'fetch("https://api.github.com/zen") — press ▶',
    value: 'tap to run',
    onPress: async () => {
      const dom = JSDOM.create(html, { onFetch });
      const res = await dom.evaluate(`
        (async () => {
          const res = await fetch('https://api.github.com/zen');
          const text = await res.text();
          return JSON.stringify({ status: res.status, ok: res.ok, text });
        })()
      `);
      dom.dispose();
      return `✓ ${res}`;
    },
  });

  return { title: 'Fetch', results };
};
