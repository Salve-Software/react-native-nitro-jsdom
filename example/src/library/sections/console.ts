import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const consoleSection = async (): Promise<ISection> => {
  const html = `<html><body></body></html>`;
  const results = [];

  const logs: string[] = [];
  const dom = JSDOM.create(html, {
    onConsole: (level, args) => { logs.push(`[${level}] ${args.join(' ')}`); },
  });
  await dom.evaluate(`console.log('hello', 42)`);
  results.push({ label: 'console.log forwarded — expect: [log] hello 42', value: logs[0] ?? '(none)' });
  dom.dispose();

  const silentDom = JSDOM.create(html);
  let silentOk = true;
  try {
    await silentDom.evaluate(`console.log('silent')`);
  } catch {
    silentOk = false;
  }
  results.push({ label: 'onConsole absent → silent — expect: true', value: String(silentOk) });
  silentDom.dispose();

  return { title: 'Console', results };
};
