import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const customEventSection = async (): Promise<ISection> => {
  const html = `<html><body><div id="target"></div></body></html>`;
  const results = [];

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`typeof CustomEvent`);
    results.push({ label: 'CustomEvent defined — expect: function', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      const evt = new CustomEvent('greet', { detail: { name: 'Ada' } });
      JSON.stringify({ type: evt.type, detail: evt.detail });
    `);
    results.push({ label: 'type + detail — expect: {"type":"greet","detail":{"name":"Ada"}}', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`
      new Promise(resolve => {
        const el = document.getElementById('target');
        el.addEventListener('greet', (evt) => resolve(evt.detail.name));
        el.dispatchEvent(new CustomEvent('greet', { detail: { name: 'Grace' } }));
      })
    `);
    results.push({ label: 'dispatchEvent delivers detail to listener — expect: Grace', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`String(new CustomEvent('x').detail)`);
    results.push({ label: 'detail defaults to undefined — expect: undefined', value: res });
    dom.dispose();
  }

  return { title: 'CustomEvent', results };
};
