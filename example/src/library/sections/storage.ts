import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const storageSection = async (): Promise<ISection> => {
  const html = `<html><body></body></html>`;
  const results = [];

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      localStorage.setItem('name', 'Ada');
      localStorage.getItem('name');
    `);
    results.push({ label: 'localStorage set/get — expect: Ada', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`String(localStorage.getItem('missing'))`);
    results.push({ label: 'localStorage.getItem missing key — expect: null', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      localStorage.setItem('a', '1');
      localStorage.setItem('b', '2');
      const before = localStorage.length;
      localStorage.removeItem('a');
      JSON.stringify({ before, after: localStorage.length, remaining: localStorage.getItem('a') });
    `);
    results.push({ label: 'removeItem + length — expect: {"before":2,"after":1,"remaining":null}', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      localStorage.setItem('x', '1');
      localStorage.clear();
      String(localStorage.length)
    `);
    results.push({ label: 'clear() empties storage — expect: 0', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      localStorage.setItem('shared', 'nope');
      String(sessionStorage.getItem('shared'))
    `);
    results.push({ label: 'localStorage/sessionStorage are independent — expect: null', value: res });
    dom.dispose();
  }

  return { title: 'Storage', results };
};
