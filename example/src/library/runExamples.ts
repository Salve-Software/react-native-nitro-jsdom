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

  // ── v0.5 MutationObserver ────────────────────────────────────────────────────

  const moDom = JSDOM.create(`
    <html>
      <body>
        <ul id="mo-list"></ul>
        <div id="mo-target" class="initial" data-x="hello">text</div>
      </body>
    </html>
  `);

  // Criterion 1: MutationObserver constructor is defined
  const moExists = await moDom.evaluate(`typeof MutationObserver`);
  results.push({ label: 'MutationObserver defined — expect: function', value: moExists });

  // Criterion 2: childList — detect child addition
  const childListResult = await moDom.evaluate(`
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
  results.push({ label: 'MutationObserver childList add — expect: childList:1', value: childListResult });

  // Criterion 3: attributes — detect attribute change
  const attrResult = await moDom.evaluate(`
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
  results.push({ label: 'MutationObserver attributes — expect: attributes:data-x', value: attrResult });

  // Criterion 7: attributeOldValue
  const attrOldValResult = await moDom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      const observer = new MutationObserver((records) => {
        const r = records[0];
        resolve(r.oldValue);
      });
      observer.observe(el, { attributes: true, attributeOldValue: true });
      el.setAttribute('data-x', 'changed');
    })
  `);
  results.push({ label: 'MutationObserver attributeOldValue — expect: world', value: attrOldValResult });

  // Criterion 6: attributeFilter — only fires for listed attributes
  const filterResult = await moDom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let fired = false;
      const observer = new MutationObserver(() => { fired = true; });
      observer.observe(el, { attributes: true, attributeFilter: ['class'] });
      el.setAttribute('data-x', 'ignored'); // should NOT fire (not in filter)
      el.setAttribute('class', 'new-class'); // SHOULD fire
      // Wait one microtask cycle
      Promise.resolve().then(() => resolve(String(fired)));
    })
  `);
  results.push({ label: 'MutationObserver attributeFilter — expect: true', value: filterResult });

  // Criterion 5: subtree — observe body, mutate deep descendant
  const subtreeResult = await moDom.evaluate(`
    new Promise(resolve => {
      const observer = new MutationObserver((records) => {
        resolve('subtree:' + records[0].type);
      });
      observer.observe(document.body, { childList: true, subtree: true });
      const list = document.getElementById('mo-list');
      const li = document.createElement('li');
      list.appendChild(li);
    })
  `);
  results.push({ label: 'MutationObserver subtree — expect: subtree:childList', value: subtreeResult });

  // Criterion 10: disconnect — stops delivery and discards pending records
  const disconnectResult = await moDom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let callCount = 0;
      const observer = new MutationObserver(() => { callCount++; });
      observer.observe(el, { attributes: true });
      el.setAttribute('data-x', 'before-disconnect');
      observer.disconnect();
      el.setAttribute('data-x', 'after-disconnect');
      // After microtask drain, callCount should still be 0
      Promise.resolve().then(() => resolve(String(callCount)));
    })
  `);
  results.push({ label: 'MutationObserver disconnect — expect: 0', value: disconnectResult });

  // Criterion 11: takeRecords — flush and return without calling callback
  const takeRecordsResult = await moDom.evaluate(`
    (() => {
      const el = document.getElementById('mo-target');
      let callbackFired = false;
      const observer = new MutationObserver(() => { callbackFired = true; });
      observer.observe(el, { attributes: true });
      el.setAttribute('data-x', 'snapshot');
      const records = observer.takeRecords();
      observer.disconnect();
      // records should have 1 item; callback should NOT have fired
      return records.length + ':' + String(callbackFired);
    })()
  `);
  results.push({ label: 'MutationObserver takeRecords — expect: 1:false', value: takeRecordsResult });

  // Criterion 9: multiple observers on same node
  const multiObsResult = await moDom.evaluate(`
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
  results.push({ label: 'MutationObserver multi-observer — expect: both:2', value: multiObsResult });

  // Criterion 8: characterDataOldValue
  const charDataOldValResult = await moDom.evaluate(`
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
  results.push({ label: 'MutationObserver characterDataOldValue — expect: characterData:original text', value: charDataOldValResult });

  // Criterion 12: microtask ordering — MutationObserver fires before setTimeout
  // GAP-2: add a fallback timeout so the test cannot hang if MO callback fails to fire
  const orderResult = await moDom.evaluate(`
    new Promise(resolve => {
      const el = document.getElementById('mo-target');
      let log = [];
      const observer = new MutationObserver(() => { log.push('mo'); });
      observer.observe(el, { attributes: true });
      // Fallback: resolve after 1s in case MO callback never fires
      const fallback = setTimeout(() => { resolve('timeout:' + log.join(',')); }, 1000);
      setTimeout(() => {
        clearTimeout(fallback);
        log.push('timer');
        resolve(log.join(','));
      }, 0);
      el.setAttribute('data-x', 'ordering');
    })
  `);
  results.push({ label: 'MutationObserver fires before setTimeout — expect: mo,timer', value: orderResult });

  moDom.dispose();

  // ── v0.5 window.alert / confirm / prompt ────────────────────────────────────

  // alert with callback — captured message matches, result is "undefined"
  {
    let captured = '';
    const dom = JSDOM.create('<html><body></body></html>', {
      onAlert: (m) => { captured = m; },
    });
    const res = await dom.evaluate(`window.alert('hello')`);
    results.push({ label: 'alert with callback — captured — expect: hello', value: captured });
    results.push({ label: 'alert returns undefined — expect: undefined', value: res });
    dom.dispose();
  }

  // alert without callback — no throw, returns undefined
  {
    const dom = JSDOM.create('<html><body></body></html>');
    let ok = true;
    let res = '';
    try {
      res = await dom.evaluate(`window.alert('hello'); 1`);
    } catch {
      ok = false;
    }
    results.push({ label: 'alert without callback — no throw — expect: true', value: String(ok) });
    results.push({ label: 'alert without callback — returns 1 — expect: 1', value: res });
    dom.dispose();
  }

  // confirm with callback returning true
  {
    const dom = JSDOM.create('<html><body></body></html>', {
      onConfirm: () => true,
    });
    const res = await dom.evaluate(`window.confirm('ok?')`);
    results.push({ label: 'confirm with callback true — expect: true', value: res });
    dom.dispose();
  }

  // confirm with callback returning false
  {
    const dom = JSDOM.create('<html><body></body></html>', {
      onConfirm: () => false,
    });
    const res = await dom.evaluate(`window.confirm('ok?')`);
    results.push({ label: 'confirm with callback false — expect: false', value: res });
    dom.dispose();
  }

  // confirm without callback — returns false (browser default)
  {
    const dom = JSDOM.create('<html><body></body></html>');
    const res = await dom.evaluate(`window.confirm('ok?')`);
    results.push({ label: 'confirm without callback — expect: false', value: res });
    dom.dispose();
  }

  // prompt with callback returning a string
  {
    const dom = JSDOM.create('<html><body></body></html>', {
      onPrompt: (_m, d) => (d ?? '') + '!',
    });
    const res = await dom.evaluate(`window.prompt('name?', 'default')`);
    results.push({ label: 'prompt with callback — expect: default!', value: res });
    dom.dispose();
  }

  // prompt with callback returning null
  {
    const dom = JSDOM.create('<html><body></body></html>', {
      onPrompt: () => null,
    });
    const res = await dom.evaluate(`window.prompt('x')`);
    results.push({ label: 'prompt callback returning null — expect: null', value: res });
    dom.dispose();
  }

  // prompt without callback — returns null (browser default)
  {
    const dom = JSDOM.create('<html><body></body></html>');
    const res = await dom.evaluate(`window.prompt('x')`);
    results.push({ label: 'prompt without callback — expect: null', value: res });
    dom.dispose();
  }

  // prompt defaultValue forwarding — absent vs present
  {
    const dom = JSDOM.create('<html><body></body></html>', {
      onPrompt: (_m, d) => d === undefined ? '<none>' : d,
    });
    const resAbsent = await dom.evaluate(`window.prompt('x')`);
    const resPresent = await dom.evaluate(`window.prompt('x', 'abc')`);
    results.push({ label: 'prompt defaultValue absent — expect: <none>', value: resAbsent });
    results.push({ label: 'prompt defaultValue present — expect: abc', value: resPresent });
    dom.dispose();
  }

  // synchronous ordering — callback fires during evaluate(), before resume
  // Uses a single evaluate() call: if onAlert fires synchronously during the
  // JS execution, order[0] will be 'alert' before the promise resolves.
  {
    const order: string[] = [];
    const dom = JSDOM.create('<html><body></body></html>', {
      onAlert: () => order.push('alert'),
    });
    await dom.evaluate(`window.alert('x')`);
    // onAlert must have fired synchronously inside the evaluate() call.
    // If it had fired asynchronously (after resolve), order would be empty here.
    results.push({ label: 'alert sync ordering — expect: alert', value: order[0] ?? 'not-fired' });
    dom.dispose();
  }

  // onConsole regression — existing test still works
  {
    const consoleLogs2: string[] = [];
    const dom = JSDOM.create('<html><body></body></html>', {
      onConsole: (_level, args) => { consoleLogs2.push(args.join(' ')); },
    });
    await dom.evaluate(`console.log('regression', 'check')`);
    results.push({ label: 'onConsole regression — expect: regression check', value: consoleLogs2[0] ?? '(none)' });
    dom.dispose();
  }

  return results;
}
