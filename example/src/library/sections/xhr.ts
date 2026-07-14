import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Bridges the sandbox's XMLHttpRequest to React Native's real fetch(), same as fetch.ts.
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

export const xhrSection = async (): Promise<ISection> => {
  const html = `<html><body></body></html>`;
  const results = [];

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      const xhr = new XMLHttpRequest();
      xhr.open('GET', 'https://example.com');
      xhr.send();
      JSON.stringify({ status: xhr.status, statusText: xhr.statusText, readyState: xhr.readyState });
    `);
    results.push({ label: 'XHR without onFetch — expect: {"status":0,"statusText":"","readyState":4}', value: res });
    dom.dispose();
  }

  results.push({
    label: 'XHR GET https://api.github.com/zen — press ▶',
    value: 'tap to run',
    onPress: async () => {
      const dom = JSDOM.create(html, { onFetch });
      const res = await dom.evaluate(`
        const xhr = new XMLHttpRequest();
        xhr.open('GET', 'https://api.github.com/zen');
        xhr.send();
        JSON.stringify({ status: xhr.status, text: xhr.responseText });
      `);
      dom.dispose();
      return `✓ ${res}`;
    },
  });

  {
    const dom = JSDOM.create(html, { onFetch });
    const res = await dom.evaluate(`
      const xhr = new XMLHttpRequest();
      xhr.open('GET', 'https://api.github.com/zen');
      xhr.setRequestHeader('X-Test-Header', 'nitro-jsdom');
      xhr.send();
      xhr.getResponseHeader('content-type') !== null ? 'has-content-type' : 'no-content-type';
    `);
    results.push({ label: 'setRequestHeader + getResponseHeader — expect: has-content-type', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { onFetch });
    const res = await dom.evaluate(`
      const xhr = new XMLHttpRequest();
      const seen = [];
      xhr.onreadystatechange = () => seen.push(xhr.readyState);
      xhr.open('GET', 'https://api.github.com/zen');
      xhr.send();
      JSON.stringify(seen);
    `);
    results.push({ label: 'readyState sequence via onreadystatechange — expect: [1,2,3,4]', value: res });
    dom.dispose();
  }

  return { title: 'XMLHttpRequest', results };
};
