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
});
