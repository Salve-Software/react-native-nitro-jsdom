import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Real-world CMS-widget style scenarios: HTML payloads with a small embedded
// script that computes something before it's shown (countdown timer,
// personalized greeting, discount badge), plus common patterns apps reach
// for when rendering server-driven HTML (forms, lists, templating).
// Runs on a real device/simulator via react-native-harness.

describe('Real-world scenarios', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('countdown timer widget computes remaining time from a target date', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="countdown" data-target="2030-01-01T00:00:00Z">--</div>
        <script>
          const el = document.getElementById('countdown');
          const target = new Date(el.dataset.target).getTime();
          const now = new Date('2029-12-31T00:00:00Z').getTime();
          const diffMs = target - now;
          const days = Math.floor(diffMs / (1000 * 60 * 60 * 24));
          el.textContent = days + ' days left';
        </script>
      </body></html>
    `);
    const text = await dom.evaluate(`document.getElementById('countdown').textContent`);
    expect(text).toBe('1 days left');
  });

  it('personalized greeting widget reads injected user data and interpolates it', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="greeting"></div>
        <script>
          window.__USER__ = { name: 'Angelo', vipTier: 'gold' };
          const { name, vipTier } = window.__USER__;
          const hour = 14;
          const period = hour < 12 ? 'morning' : hour < 18 ? 'afternoon' : 'evening';
          document.getElementById('greeting').textContent =
            \`Good \${period}, \${name}! (\${vipTier} member)\`;
        </script>
      </body></html>
    `);
    const text = await dom.evaluate(`document.getElementById('greeting').textContent`);
    expect(text).toBe('Good afternoon, Angelo! (gold member)');
  });

  it('discount badge widget computes a price from a base value and a coupon rule', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="badge" data-base-price="199.9" data-discount-pct="15"></div>
        <script>
          const el = document.getElementById('badge');
          const base = parseFloat(el.dataset.basePrice);
          const pct = parseFloat(el.dataset.discountPct);
          const final = (base * (1 - pct / 100)).toFixed(2);
          el.textContent = '-' + pct + '% → R$ ' + final;
        </script>
      </body></html>
    `);
    const text = await dom.evaluate(`document.getElementById('badge').textContent`);
    expect(text).toBe('-15% → R$ 169.91');
  });

  it('renders a list of items from a JSON payload with a template loop', async () => {
    dom = JSDOM.create(`
      <html><body>
        <ul id="list"></ul>
        <script>
          const items = [
            { id: 1, name: 'Coffee', price: 12.5 },
            { id: 2, name: 'Tea', price: 8 },
            { id: 3, name: 'Juice', price: 15 },
          ];
          const list = document.getElementById('list');
          items.forEach((item) => {
            const li = document.createElement('li');
            li.dataset.id = String(item.id);
            li.textContent = item.name + ' - $' + item.price.toFixed(2);
            list.appendChild(li);
          });
        </script>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify(Array.from(document.querySelectorAll('#list li')).map((li) => li.textContent));
    `);
    expect(JSON.parse(result)).toEqual(['Coffee - $12.50', 'Tea - $8.00', 'Juice - $15.00']);
  });

  it('validates a login form and shows/hides an error message on submit', async () => {
    dom = JSDOM.create(`
      <html><body>
        <form id="login">
          <input id="email" value="not-an-email" />
          <span id="error" style="display:none"></span>
        </form>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const email = document.getElementById('email').value;
      const isValid = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/.test(email);
      const error = document.getElementById('error');
      if (!isValid) {
        error.textContent = 'Invalid email';
        error.style.display = 'block';
      }
      JSON.stringify({ isValid, errorText: error.textContent, errorDisplay: error.style.display });
    `);
    expect(JSON.parse(result)).toEqual({
      isValid: false,
      errorText: 'Invalid email',
      errorDisplay: 'block',
    });
  });

  it('like-button counter uses event delegation on a parent list', async () => {
    dom = JSDOM.create(`
      <html><body>
        <ul id="feed">
          <li><button class="like" data-count="0">Like (0)</button></li>
          <li><button class="like" data-count="0">Like (0)</button></li>
        </ul>
      </body></html>
    `);
    const result = await dom.evaluate(`
      document.getElementById('feed').addEventListener('click', (e) => {
        const btn = e.target.closest('.like');
        if (!btn) return;
        const next = parseInt(btn.dataset.count, 10) + 1;
        btn.dataset.count = String(next);
        btn.textContent = 'Like (' + next + ')';
      });
      const buttons = document.querySelectorAll('.like');
      buttons[0].dispatchEvent(new Event('click', { bubbles: true }));
      buttons[0].dispatchEvent(new Event('click', { bubbles: true }));
      buttons[1].dispatchEvent(new Event('click', { bubbles: true }));
      JSON.stringify(Array.from(buttons).map((b) => b.textContent));
    `);
    expect(JSON.parse(result)).toEqual(['Like (2)', 'Like (1)']);
  });

  it('parses a query-string-like data blob without URLSearchParams', async () => {
    dom = JSDOM.create(`<html><body><div id="out"></div></body></html>`);
    const result = await dom.evaluate(`
      const raw = 'utm_source=newsletter&utm_campaign=summer&ref=42';
      const parsed = Object.fromEntries(raw.split('&').map((pair) => pair.split('=')));
      document.getElementById('out').textContent = JSON.stringify(parsed);
      document.getElementById('out').textContent;
    `);
    expect(JSON.parse(result)).toEqual({
      utm_source: 'newsletter',
      utm_campaign: 'summer',
      ref: '42',
    });
  });

  it('toggles a tab UI by swapping active classes across siblings', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div class="tabs">
          <button class="tab active" data-tab="a">A</button>
          <button class="tab" data-tab="b">B</button>
          <button class="tab" data-tab="c">C</button>
        </div>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const tabs = document.querySelectorAll('.tab');
      function activate(target) {
        tabs.forEach((t) => t.classList.toggle('active', t === target));
      }
      activate(tabs[2]);
      JSON.stringify(Array.from(tabs).map((t) => ({ tab: t.dataset.tab, active: t.classList.contains('active') })));
    `);
    expect(JSON.parse(result)).toEqual([
      { tab: 'a', active: false },
      { tab: 'b', active: false },
      { tab: 'c', active: true },
    ]);
  });

  it('an uncaught error inside evaluate() rejects with a useful message', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    await expect(dom.evaluate(`document.getElementById('missing').textContent`)).rejects.toThrow();
  });

  it('a script tag that throws does not corrupt the DOM for subsequent evaluate() calls', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="safe">still here</div>
        <script>throw new Error('boom')</script>
      </body></html>
    `);
    const text = await dom.evaluate(`document.getElementById('safe').textContent`);
    expect(text).toBe('still here');
  });

  it('reads table data and computes a total, mirroring an invoice widget', async () => {
    dom = JSDOM.create(`
      <html><body>
        <table id="invoice">
          <tbody>
            <tr><td class="qty">2</td><td class="price">10.00</td></tr>
            <tr><td class="qty">3</td><td class="price">5.50</td></tr>
            <tr><td class="qty">1</td><td class="price">20.00</td></tr>
          </tbody>
        </table>
        <div id="total"></div>
      </body></html>
    `);
    const total = await dom.evaluate(`
      const rows = document.querySelectorAll('#invoice tbody tr');
      let sum = 0;
      rows.forEach((row) => {
        const qty = parseInt(row.querySelector('.qty').textContent, 10);
        const price = parseFloat(row.querySelector('.price').textContent);
        sum += qty * price;
      });
      document.getElementById('total').textContent = sum.toFixed(2);
      document.getElementById('total').textContent;
    `);
    expect(total).toBe('56.50');
  });

  // Adapted from mdn/dom-examples/url-params: builds a table of the current
  // URL's query params on window 'load', mirroring how a CMS widget reads
  // personalization data (coupon code, referral id) out of its own URL.
  it('builds a param table from the URL on window load (adapted from mdn/dom-examples/url-params)', async () => {
    dom = JSDOM.create(
      `
      <html><body>
        <pre id="url-output"></pre>
        <table class="param-table"></table>
        <script>
          function fillTableWithParameters() {
            const table = document.querySelector('.param-table');
            const outputBox = document.getElementById('url-output');
            const url = new URL(document.location.href);
            url.searchParams.sort();
            for (const key of url.searchParams.keys()) {
              const row = document.createElement('tr');
              const keyCell = document.createElement('td');
              keyCell.textContent = key;
              const valCell = document.createElement('td');
              valCell.textContent = url.searchParams.get(key);
              row.appendChild(keyCell);
              row.appendChild(valCell);
              table.appendChild(row);
            }
            outputBox.textContent = 'Current URL: ' + url;
          }
          window.addEventListener('load', fillTableWithParameters);
        </script>
      </body></html>
    `,
      { url: 'https://example.com/widget?excitement=high&from=MDN' }
    );
    const result = await dom.evaluate(`
      JSON.stringify({
        rows: Array.from(document.querySelectorAll('.param-table tr')).map((tr) =>
          Array.from(tr.querySelectorAll('td')).map((td) => td.textContent)
        ),
        output: document.getElementById('url-output').textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      rows: [
        ['excitement', 'high'],
        ['from', 'MDN'],
      ],
      output: 'Current URL: https://example.com/widget?excitement=high&from=MDN',
    });
  });

  // Adapted from mdn/dom-examples/web-storage: persists a widget's chosen
  // color/font into localStorage and reflects it back into inline styles,
  // then confirms a rebound onchange handler round-trips a new value.
  it('persists widget preferences to localStorage and reflects them into styles (adapted from mdn/dom-examples/web-storage)', async () => {
    dom = JSDOM.create(`
      <html><body>
        <p>Sample text</p>
        <input id="bgcolor" value="FF0000" />
        <select id="font">
          <option value="sans-serif" selected>Sans-serif</option>
          <option value="serif">Serif</option>
        </select>
      </body></html>
    `);
    const first = await dom.evaluate(`
      const htmlElem = document.querySelector('html');
      const pElem = document.querySelector('p');
      const bgcolorInput = document.getElementById('bgcolor');
      const fontSelect = document.getElementById('font');

      function setStyles() {
        htmlElem.style.backgroundColor = '#' + localStorage.getItem('bgcolor');
        pElem.style.fontFamily = localStorage.getItem('font');
      }
      function populateStorage() {
        localStorage.setItem('bgcolor', bgcolorInput.value);
        localStorage.setItem('font', fontSelect.value);
        setStyles();
      }
      if (!localStorage.getItem('bgcolor')) {
        populateStorage();
      } else {
        setStyles();
      }
      bgcolorInput.onchange = populateStorage;

      JSON.stringify({
        storedColor: localStorage.getItem('bgcolor'),
        storedFont: localStorage.getItem('font'),
        htmlBg: htmlElem.style.backgroundColor,
        pFont: pElem.style.fontFamily,
      });
    `);
    expect(JSON.parse(first)).toEqual({
      storedColor: 'FF0000',
      storedFont: 'sans-serif',
      htmlBg: '#FF0000',
      pFont: 'sans-serif',
    });

    const second = await dom.evaluate(`
      bgcolorInput.value = '00FF00';
      bgcolorInput.dispatchEvent(new Event('change'));
      JSON.stringify({
        storedColor: localStorage.getItem('bgcolor'),
        htmlBg: document.querySelector('html').style.backgroundColor,
      });
    `);
    expect(JSON.parse(second)).toEqual({ storedColor: '00FF00', htmlBg: '#00FF00' });
  });

  // Adapted from mdn/dom-examples/insert-adjacent/insertAdjacentElement.html:
  // clicking a box selects it, then "insert before"/"insert after" add a new
  // box next to the selection — the same "insert a sibling next to the
  // clicked item" pattern a CMS widget uses to grow a list around a click.
  it('click-to-select then insert-before/after grows a box list around the selection (adapted from mdn/dom-examples/insert-adjacent)', async () => {
    dom = JSDOM.create(`
      <html><body>
        <section>
          <div class="box" data-id="0"></div>
          <div class="box" data-id="1"></div>
          <div class="box" data-id="2"></div>
        </section>
        <button class="before">Insert before</button>
        <button class="after">Insert after</button>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const container = document.querySelector('section');
      let activeElem;
      let nextId = 3;

      function setListener(elem) {
        elem.addEventListener('click', () => { activeElem = elem; });
      }
      Array.from(container.querySelectorAll('.box')).forEach(setListener);

      document.querySelector('.before').addEventListener('click', () => {
        const box = document.createElement('div');
        box.className = 'box';
        box.dataset.id = String(nextId++);
        if (activeElem) activeElem.insertAdjacentElement('beforebegin', box);
        setListener(box);
      });
      document.querySelector('.after').addEventListener('click', () => {
        const box = document.createElement('div');
        box.className = 'box';
        box.dataset.id = String(nextId++);
        if (activeElem) activeElem.insertAdjacentElement('afterend', box);
        setListener(box);
      });

      container.querySelectorAll('.box')[1].dispatchEvent(new Event('click'));
      document.querySelector('.before').dispatchEvent(new Event('click'));
      document.querySelector('.after').dispatchEvent(new Event('click'));

      JSON.stringify(Array.from(container.querySelectorAll('.box')).map((b) => b.dataset.id));
    `);
    expect(JSON.parse(result)).toEqual(['0', '3', '1', '4', '2']);
  });

  // Adapted from mdn/dom-examples/mediaquerylist: a responsive widget that
  // reacts to matchMedia() and wires both addEventListener('change') and the
  // legacy .onchange property, common in CMS widgets that adapt their layout.
  it('reacts to matchMedia() results and wires both addEventListener and onchange (adapted from mdn/dom-examples/mediaquerylist)', async () => {
    dom = JSDOM.create(`
      <html><body>
        <p></p>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const para = document.querySelector('p');
      const mql = window.matchMedia('(max-width: 600px)');

      function screenTest(e) {
        if (e.matches) {
          para.textContent = 'narrow screen';
          document.body.style.backgroundColor = 'red';
        } else {
          para.textContent = 'wide screen';
          document.body.style.backgroundColor = 'blue';
        }
      }

      screenTest(mql);
      mql.addEventListener('change', screenTest);
      let onchangeAssigned = false;
      mql.onchange = function () { onchangeAssigned = true; };

      JSON.stringify({
        text: para.textContent,
        bg: document.body.style.backgroundColor,
        media: mql.media,
        onchangeIsFunction: typeof mql.onchange === 'function',
      });
    `);
    expect(JSON.parse(result)).toEqual({
      text: 'wide screen',
      bg: 'blue',
      media: '(max-width: 600px)',
      onchangeIsFunction: true,
    });
  });

  // Adapted from mdn/dom-examples/css-progress: reads layout geometry and
  // writes it back as a CSS custom property, the "measure then react" pattern
  // a widget uses even though there's no real layout engine behind the stub.
  it('reads getBoundingClientRect() and writes it back as a CSS custom property (adapted from mdn/dom-examples/css-progress)', async () => {
    dom = JSDOM.create(`
      <html><body>
        <article></article>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const articleElem = document.querySelector('article');
      function setContainerWidth() {
        const clientWidth = articleElem.getBoundingClientRect().width;
        articleElem.style.setProperty('--container-width', Math.floor(clientWidth) + 'px');
      }
      setContainerWidth();
      articleElem.style.getPropertyValue('--container-width');
    `);
    expect(result).toBe('0px');
  });
});
