import type { ISection } from '../../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const mutationObserverSection = async (): Promise<ISection> => {
  const dom = JSDOM.create(`
    <html>
      <body>
        <ul id="mo-list"></ul>
        <div id="mo-target" class="initial" data-x="hello">text</div>
      </body>
    </html>
  `);
  const results = [];

  const moExists = await dom.evaluate(`typeof MutationObserver`);
  results.push({ label: 'MutationObserver defined — expect: function', value: moExists });

  const childListResult = await dom.evaluate(`
    new Promise(resolve => {
      const list = document.getElementById('mo-list');
      const observer = new MutationObserver((records) => {
        const r = records[0];
        resolve(r.type + ':' + r.addedNodes.length);
      });
      observer.observe(list, { childList: true });
      const li = document.createElement('li');
      li.textContent = 'item';
      list.appendChild(li);
    })
  `);
  results.push({ label: 'childList add — expect: childList:1', value: childListResult });

  const attrResult = await dom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      const observer = new MutationObserver((records) => {
        const r = records[0];
        resolve(r.type + ':' + r.attributeName);
      });
      observer.observe(el, { attributes: true });
      el.setAttribute('data-x', 'world');
    })
  `);
  results.push({ label: 'attributes — expect: attributes:data-x', value: attrResult });

  const attrOldValResult = await dom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      const observer = new MutationObserver((records) => {
        resolve(records[0].oldValue);
      });
      observer.observe(el, { attributes: true, attributeOldValue: true });
      el.setAttribute('data-x', 'changed');
    })
  `);
  results.push({ label: 'attributeOldValue — expect: world', value: attrOldValResult });

  const filterResult = await dom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let fired = false;
      const observer = new MutationObserver(() => { fired = true; });
      observer.observe(el, { attributes: true, attributeFilter: ['class'] });
      el.setAttribute('data-x', 'ignored');
      el.setAttribute('class', 'new-class');
      Promise.resolve().then(() => resolve(String(fired)));
    })
  `);
  results.push({ label: 'attributeFilter — expect: true', value: filterResult });

  const subtreeResult = await dom.evaluate(`
    new Promise(resolve => {
      const observer = new MutationObserver((records) => {
        resolve('subtree:' + records[0].type);
      });
      observer.observe(document.body, { childList: true, subtree: true });
      const li = document.createElement('li');
      document.getElementById('mo-list').appendChild(li);
    })
  `);
  results.push({ label: 'subtree — expect: subtree:childList', value: subtreeResult });

  const disconnectResult = await dom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let callCount = 0;
      const observer = new MutationObserver(() => { callCount++; });
      observer.observe(el, { attributes: true });
      el.setAttribute('data-x', 'before-disconnect');
      observer.disconnect();
      el.setAttribute('data-x', 'after-disconnect');
      Promise.resolve().then(() => resolve(String(callCount)));
    })
  `);
  results.push({ label: 'disconnect — expect: 0', value: disconnectResult });

  const takeRecordsResult = await dom.evaluate(`
    (() => {
      const el = document.getElementById('mo-target');
      let callbackFired = false;
      const observer = new MutationObserver(() => { callbackFired = true; });
      observer.observe(el, { attributes: true });
      el.setAttribute('data-x', 'snapshot');
      const records = observer.takeRecords();
      observer.disconnect();
      return records.length + ':' + String(callbackFired);
    })()
  `);
  results.push({ label: 'takeRecords — expect: 1:false', value: takeRecordsResult });

  const multiObsResult = await dom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let count = 0;
      const obs1 = new MutationObserver(() => { count++; if (count === 2) resolve('both:' + count); });
      const obs2 = new MutationObserver(() => { count++; if (count === 2) resolve('both:' + count); });
      obs1.observe(el, { attributes: true });
      obs2.observe(el, { attributes: true });
      el.setAttribute('data-x', 'multi');
    })
  `);
  results.push({ label: 'multi-observer — expect: both:2', value: multiObsResult });

  const charDataOldValResult = await dom.evaluate(`
    new Promise(resolve => {
      const parent = document.getElementById('mo-target');
      const textNode = document.createTextNode('original text');
      parent.appendChild(textNode);
      const observer = new MutationObserver((records) => {
        const r = records[0];
        resolve(r.type + ':' + (r.oldValue ?? 'null'));
      });
      observer.observe(textNode, { characterData: true, characterDataOldValue: true });
      textNode.textContent = 'changed text';
    })
  `);
  results.push({ label: 'characterDataOldValue — expect: characterData:original text', value: charDataOldValResult });

  const orderResult = await dom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let log = [];
      const observer = new MutationObserver(() => { log.push('mo'); });
      observer.observe(el, { attributes: true });
      const fallback = setTimeout(() => { resolve('timeout:' + log.join(',')); }, 1000);
      setTimeout(() => {
        clearTimeout(fallback);
        log.push('timer');
        resolve(log.join(','));
      }, 0);
      el.setAttribute('data-x', 'ordering');
    })
  `);
  results.push({ label: 'fires before setTimeout — expect: mo,timer', value: orderResult });

  dom.dispose();

  return { title: 'MutationObserver', results };
};
