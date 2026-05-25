import type { IResult } from '../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const runExamples = async (): Promise<IResult[]> => {
  const html = `
    <html>
      <body>
        <div id="result">0</div>
        <p class="item active">Hello</p>
        <p class="item">World</p>
        <ul id="list"></ul>
      </body>
    </html>
  `

  const dom = JSDOM.create(html);
  const results: IResult[] = [];

  // getElementById + textContent setter/getter
  await dom.evaluate(`document.getElementById('result').textContent = String(2 + 2)`);
  const textContent = await dom.evaluate(`document.getElementById('result').textContent`);
  results.push({ label: 'getElementById → textContent — expect: 4', value: textContent });

  // querySelector + setAttribute
  await dom.evaluate(`document.querySelector('.item').setAttribute('data-index', '0')`);
  const attr = await dom.evaluate(`document.querySelector('.item').getAttribute('data-index')`);
  results.push({ label: 'querySelector → getAttribute — expect: 0', value: attr });

  // querySelectorAll + length
  const count = await dom.evaluate(`document.querySelectorAll('.item').length`);
  results.push({ label: 'querySelectorAll → length — expect: 2', value: count });

  // createElement + appendChild + textContent
  await dom.evaluate(`
    const li = document.createElement('li');
    li.textContent = 'novo item';
    document.getElementById('list').appendChild(li);
  `);
  const listHTML = await dom.evaluate(`document.getElementById('list').innerHTML`);
  results.push({ label: 'createElement + appendChild → innerHTML — expect: <li>novo item</li>', value: listHTML });

  // classList.add + classList.contains
  await dom.evaluate(`document.querySelector('.item').classList.add('highlighted')`);
  const hasClass = await dom.evaluate(`document.querySelector('.item').classList.contains('highlighted')`);
  results.push({ label: 'classList.add → classList.contains — expect: true', value: hasClass });

  // classList.remove
  await dom.evaluate(`document.querySelector('.item').classList.remove('active')`);
  const className = await dom.evaluate(`document.querySelector('.item').className`);
  results.push({ label: 'classList.remove → className — expect: item highlighted', value: className });

  // tagName + id
  const tagName = await dom.evaluate(`document.getElementById('result').tagName`);
  results.push({ label: 'tagName — expect: DIV', value: tagName });

  // parentElement
  const parentTag = await dom.evaluate(`document.getElementById('result').parentElement.tagName`);
  results.push({ label: 'parentElement.tagName — expect: BODY', value: parentTag });

  // children length
  const childCount = await dom.evaluate(`document.body.children.length`);
  results.push({ label: 'document.body.children.length — expect: 4', value: childCount });

  // innerHTML setter
  await dom.evaluate(`document.getElementById('result').innerHTML = '<b>bold</b>'`);
  const innerResult = await dom.evaluate(`document.getElementById('result').innerHTML`);
  results.push({ label: 'innerHTML setter → getter — expect: <b>bold</b>', value: innerResult });

  // remove()
  await dom.evaluate(`document.querySelector('.item').remove()`);
  const countAfterRemove = await dom.evaluate(`document.querySelectorAll('.item').length`);
  results.push({ label: 'remove() → querySelectorAll length — expect: 1', value: countAfterRemove });

  // matches()
  const matches = await dom.evaluate(`document.querySelector('.item').matches('.item')`);
  results.push({ label: 'matches(".item") — expect: true', value: matches });

  // serialize reflects all mutations
  const serialized = dom.serialize();
  results.push({ label: 'serialize() length', value: `${serialized.length} chars` });

  dom.dispose();

  // runScripts: true — <script> tags execute automatically on create()
  const htmlWithScript = `
    <html>
      <body>
        <div id="output">pending</div>
        <script>
          document.getElementById('output').textContent = 'script ran';
        </script>
      </body>
    </html>
  `;

  const domWithScripts = JSDOM.create(htmlWithScript, { runScripts: true });
  const scriptResult = await domWithScripts.evaluate(`document.getElementById('output').textContent`);
  results.push({ label: 'runScripts: true → textContent — expect: script ran', value: scriptResult });
  domWithScripts.dispose();

  // ── v0.3 Async & Events ──────────────────────────────────────────────────────

  const asyncHtml = `<html><body><div id="out">init</div><button id="btn">click me</button></body></html>`;
  const asyncDom = JSDOM.create(asyncHtml);

  // setTimeout fires before evaluate() returns
  await asyncDom.evaluate(`
    let x = 0;
    setTimeout(() => { x = 1; }, 50);
  `);
  const afterTimeout = await asyncDom.evaluate(`x`);
  results.push({ label: 'setTimeout fires before evaluate() — expect: 1', value: afterTimeout });

  // setInterval + clearInterval
  await asyncDom.evaluate(`
    let count = 0;
    const id = setInterval(() => { count++ }, 10);
    setTimeout(() => { clearInterval(id); }, 55);
  `);
  const intervalCount = await asyncDom.evaluate(`count`);
  results.push({ label: 'setInterval + clearInterval — expect: ≥1', value: intervalCount });

  // Promise.resolve
  const promiseResult = await asyncDom.evaluate(`
    new Promise(resolve => resolve('done'))
  `);
  results.push({ label: 'Promise.resolve — expect: done', value: promiseResult });

  // async/await inside evaluate
  const asyncAwaitResult = await asyncDom.evaluate(`
    (async () => {
      await new Promise(r => setTimeout(r, 10));
      return 'ok';
    })()
  `);
  results.push({ label: 'async/await — expect: ok', value: asyncAwaitResult });

  // Uncaught Promise rejection rejects evaluate()
  let rejectionCaught = false;
  try {
    await asyncDom.evaluate(`Promise.reject(new Error('intentional rejection'))`);
  } catch {
    rejectionCaught = true;
  }
  results.push({ label: 'uncaught rejection rejects evaluate() — expect: true', value: String(rejectionCaught) });

  // addEventListener + dispatchEvent
  await asyncDom.evaluate(`
    const btn = document.getElementById('btn');
    btn.addEventListener('click', () => {
      document.getElementById('out').textContent = 'clicked';
    });
    btn.dispatchEvent(new Event('click'));
  `);
  const clickResult = await asyncDom.evaluate(`document.getElementById('out').textContent`);
  results.push({ label: 'addEventListener + dispatchEvent — expect: clicked', value: clickResult });

  asyncDom.dispose();

  // console.log forwarding via onConsole
  const consoleLogs: string[] = [];
  const consoleDom = JSDOM.create(asyncHtml, {
    onConsole: (level, args) => {
      consoleLogs.push(`[${level}] ${args.join(' ')}`);
    },
  });
  await consoleDom.evaluate(`console.log('hello', 42)`);
  results.push({ label: 'console.log forwarded — expect: [log] hello 42', value: consoleLogs[0] ?? '(none)' });
  consoleDom.dispose();

  // onConsole absent → silent (no throw)
  const silentDom = JSDOM.create(asyncHtml);
  let silentOk = true;
  try {
    await silentDom.evaluate(`console.log('silent')`);
  } catch {
    silentOk = false;
  }
  results.push({ label: 'onConsole absent → silent — expect: true', value: String(silentOk) });
  silentDom.dispose();

  // dispose() with pending timers — no crash
  const disposeDom = JSDOM.create(asyncHtml);
  let disposeOk = true;
  try {
    await disposeDom.evaluate(`
      let _disposeTestId = setInterval(() => {}, 50);
      setTimeout(() => clearInterval(_disposeTestId), 10);
    `);
    disposeDom.dispose();
  } catch {
    disposeOk = false;
  }
  results.push({ label: 'dispose with pending timers — expect: true', value: String(disposeOk) });

  return results;
}
