import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const timersSection = async (): Promise<ISection> => {
  const html = `<html><body><div id="out">init</div><button id="btn">click me</button></body></html>`;
  const dom = JSDOM.create(html);
  const results = [];

  await dom.evaluate(`
    let x = 0;
    setTimeout(() => { x = 1; }, 50);
  `);
  const afterTimeout = await dom.evaluate(`x`);
  results.push({ label: 'setTimeout fires before evaluate() — expect: 1', value: afterTimeout });

  await dom.evaluate(`
    let count = 0;
    const id = setInterval(() => { count++ }, 10);
    setTimeout(() => { clearInterval(id); }, 55);
  `);
  const intervalCount = await dom.evaluate(`count`);
  results.push({ label: 'setInterval + clearInterval — expect: ≥1', value: intervalCount });

  const promiseResult = await dom.evaluate(`new Promise(resolve => resolve('done'))`);
  results.push({ label: 'Promise.resolve — expect: done', value: promiseResult });

  const asyncAwaitResult = await dom.evaluate(`
    (async () => {
      await new Promise(r => setTimeout(r, 10));
      return 'ok';
    })()
  `);
  results.push({ label: 'async/await — expect: ok', value: asyncAwaitResult });

  let rejectionCaught = false;
  try {
    await dom.evaluate(`Promise.reject(new Error('intentional rejection'))`);
  } catch {
    rejectionCaught = true;
  }
  results.push({ label: 'uncaught rejection rejects evaluate() — expect: true', value: String(rejectionCaught) });

  await dom.evaluate(`
    const btn = document.getElementById('btn');
    btn.addEventListener('click', () => {
      document.getElementById('out').textContent = 'clicked';
    });
    btn.dispatchEvent(new Event('click'));
  `);
  const clickResult = await dom.evaluate(`document.getElementById('out').textContent`);
  results.push({ label: 'addEventListener + dispatchEvent — expect: clicked', value: clickResult });

  dom.dispose();

  const disposeDom = JSDOM.create(html);
  let disposeOk = true;
  try {
    await disposeDom.evaluate(`
      let _id = setInterval(() => {}, 50);
      setTimeout(() => clearInterval(_id), 10);
    `);
    disposeDom.dispose();
  } catch {
    disposeOk = false;
  }
  results.push({ label: 'dispose with pending timers — expect: true', value: String(disposeOk) });

  return { title: 'Timers & Events', results };
};
