import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM Intl.NumberFormat/DateTimeFormat', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('Intl.NumberFormat formats currency for en-US and pt-BR with locale-correct grouping/decimal/symbol placement', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        usd: new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD' }).format(1234.5),
        brl: new Intl.NumberFormat('pt-BR', { style: 'currency', currency: 'BRL' }).format(1234.5),
        plainEn: new Intl.NumberFormat('en-US').format(1234567.891),
        plainPt: new Intl.NumberFormat('pt-BR').format(1234567.891),
        percent: new Intl.NumberFormat('en-US', { style: 'percent' }).format(0.4567),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      usd: '$1,234.50',
      brl: 'R$ 1.234,50',
      plainEn: '1,234,567.891',
      plainPt: '1.234.567,891',
      percent: '46%',
    });
  });

  it('Intl.NumberFormat respects minimumFractionDigits/maximumFractionDigits/useGrouping', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        fixed2: new Intl.NumberFormat('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(5),
        noGrouping: new Intl.NumberFormat('en-US', { useGrouping: false }).format(1234567),
        trimsTrailingZeros: new Intl.NumberFormat('en-US').format(5.1),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      fixed2: '5.00',
      noGrouping: '1234567',
      trimsTrailingZeros: '5.1',
    });
  });

  it('Intl.NumberFormat requires an explicit currency and uses per-currency decimal precision', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let threw;
      try { new Intl.NumberFormat('en-US', { style: 'currency' }); threw = null; }
      catch (e) { threw = { name: e.name, isTypeError: e instanceof TypeError }; }
      JSON.stringify({
        threw,
        jpy: new Intl.NumberFormat('en-US', { style: 'currency', currency: 'JPY' }).format(1234.5),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      threw: { name: 'TypeError', isTypeError: true },
      jpy: '¥1,235',
    });
  });

  it('Intl.DateTimeFormat dateStyle/timeStyle map to distinct component sets', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const d = new Date(2026, 6, 24, 9, 5, 3);
      JSON.stringify({
        dateStyles: ['short', 'medium', 'long', 'full'].map((s) => new Intl.DateTimeFormat('en-US', { dateStyle: s }).format(d)),
        timeStyles: ['short', 'medium', 'long', 'full'].map((s) => new Intl.DateTimeFormat('en-US', { timeStyle: s }).format(d)),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      dateStyles: ['7/24/26', 'Jul 24, 2026', 'July 24, 2026', 'Friday, July 24, 2026'],
      timeStyles: ['9:05 AM', '9:05:03 AM', '9:05:03 AM', '9:05:03 AM'],
    });
  });

  it('Intl.DateTimeFormat orders date parts by locale (MDY for en, DMY for pt) and names months/weekdays', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const d = new Date(2026, 6, 24, 9, 5, 3);
      JSON.stringify({
        enLong: new Intl.DateTimeFormat('en-US', { year: 'numeric', month: 'long', day: 'numeric' }).format(d),
        ptLong: new Intl.DateTimeFormat('pt-BR', { year: 'numeric', month: 'long', day: 'numeric' }).format(d),
        enWeekday: new Intl.DateTimeFormat('en-US', { weekday: 'long', month: 'short', day: 'numeric', year: 'numeric' }).format(d),
        enNumeric: new Intl.DateTimeFormat('en-US', { year: 'numeric', month: 'numeric', day: 'numeric' }).format(d),
        ptNumeric: new Intl.DateTimeFormat('pt-BR', { year: 'numeric', month: 'numeric', day: 'numeric' }).format(d),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      enLong: 'July 24, 2026',
      ptLong: '24 julho 2026',
      enWeekday: 'Friday, Jul 24, 2026',
      enNumeric: '7/24/2026',
      ptNumeric: '24/7/2026',
    });
  });

  it('Intl.DateTimeFormat formats time with hour12 AM/PM and 24h mode', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const morning = new Date(2026, 6, 24, 9, 5, 3);
      const afternoon = new Date(2026, 6, 24, 15, 5, 3);
      JSON.stringify({
        amPm: new Intl.DateTimeFormat('en-US', { hour: 'numeric', minute: 'numeric' }).format(morning),
        pm: new Intl.DateTimeFormat('en-US', { hour: 'numeric', minute: 'numeric' }).format(afternoon),
        h24: new Intl.DateTimeFormat('en-US', { hour: '2-digit', minute: '2-digit', hour12: false }).format(afternoon),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      amPm: '9:05 AM',
      pm: '3:05 PM',
      h24: '15:05',
    });
  });

  it('Number.prototype.toLocaleString and Date.prototype.toLocaleDateString/toLocaleTimeString delegate to Intl', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const d = new Date(2026, 6, 24, 9, 5, 3);
      JSON.stringify({
        number: (1234.5).toLocaleString('en-US'),
        dateOnly: d.toLocaleDateString('en-US'),
        timeOnly: d.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: false }),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      number: '1,234.5',
      dateOnly: '7/24/2026',
      timeOnly: '09:05',
    });
  });

  it('toLocaleString/toLocaleDateString/toLocaleTimeString apply their own defaults for an empty options object', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const d = new Date(2026, 6, 24, 9, 5, 3);
      JSON.stringify({
        toLocaleString: d.toLocaleString('en-US', {}),
        toLocaleDateString: d.toLocaleDateString('en-US', {}),
        toLocaleTimeString: d.toLocaleTimeString('en-US', {}),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      toLocaleString: '7/24/2026, 9:05:03 AM',
      toLocaleDateString: '7/24/2026',
      toLocaleTimeString: '9:05:03 AM',
    });
  });

  it('falls back to English formatting for a locale with no built-in data', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        number: new Intl.NumberFormat('de-DE').format(1234.5),
        date: new Intl.DateTimeFormat('de-DE', { year: 'numeric', month: 'long', day: 'numeric' }).format(new Date(2026, 6, 24)),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      number: '1,234.5',
      date: 'July 24, 2026',
    });
  });
});
