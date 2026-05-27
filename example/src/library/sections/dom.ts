import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const domSection = async (): Promise<ISection> => {
  const html = `
    <html>
      <body>
        <div id="result">0</div>
        <p class="item active">Hello</p>
        <p class="item">World</p>
        <ul id="list"></ul>
      </body>
    </html>
  `;

  const dom = JSDOM.create(html);
  const results = [];

  await dom.evaluate(`document.getElementById('result').textContent = String(2 + 2)`);
  const textContent = await dom.evaluate(`document.getElementById('result').textContent`);
  results.push({ label: 'getElementById → textContent — expect: 4', value: textContent });

  await dom.evaluate(`document.querySelector('.item').setAttribute('data-index', '0')`);
  const attr = await dom.evaluate(`document.querySelector('.item').getAttribute('data-index')`);
  results.push({ label: 'querySelector → getAttribute — expect: 0', value: attr });

  const count = await dom.evaluate(`document.querySelectorAll('.item').length`);
  results.push({ label: 'querySelectorAll → length — expect: 2', value: count });

  await dom.evaluate(`
    const li = document.createElement('li');
    li.textContent = 'novo item';
    document.getElementById('list').appendChild(li);
  `);
  const listHTML = await dom.evaluate(`document.getElementById('list').innerHTML`);
  results.push({ label: 'createElement + appendChild → innerHTML — expect: <li>novo item</li>', value: listHTML });

  await dom.evaluate(`document.querySelector('.item').classList.add('highlighted')`);
  const hasClass = await dom.evaluate(`document.querySelector('.item').classList.contains('highlighted')`);
  results.push({ label: 'classList.add → classList.contains — expect: true', value: hasClass });

  await dom.evaluate(`document.querySelector('.item').classList.remove('active')`);
  const className = await dom.evaluate(`document.querySelector('.item').className`);
  results.push({ label: 'classList.remove → className — expect: item highlighted', value: className });

  const tagName = await dom.evaluate(`document.getElementById('result').tagName`);
  results.push({ label: 'tagName — expect: DIV', value: tagName });

  const parentTag = await dom.evaluate(`document.getElementById('result').parentElement.tagName`);
  results.push({ label: 'parentElement.tagName — expect: BODY', value: parentTag });

  const childCount = await dom.evaluate(`document.body.children.length`);
  results.push({ label: 'document.body.children.length — expect: 4', value: childCount });

  await dom.evaluate(`document.getElementById('result').innerHTML = '<b>bold</b>'`);
  const innerResult = await dom.evaluate(`document.getElementById('result').innerHTML`);
  results.push({ label: 'innerHTML setter → getter — expect: <b>bold</b>', value: innerResult });

  await dom.evaluate(`document.querySelector('.item').remove()`);
  const countAfterRemove = await dom.evaluate(`document.querySelectorAll('.item').length`);
  results.push({ label: 'remove() → querySelectorAll length — expect: 1', value: countAfterRemove });

  const matches = await dom.evaluate(`document.querySelector('.item').matches('.item')`);
  results.push({ label: 'matches(".item") — expect: true', value: matches });

  const serialized = dom.serialize();
  results.push({ label: 'serialize() length', value: `${serialized.length} chars` });

  dom.dispose();

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

  return { title: 'DOM', results };
};
