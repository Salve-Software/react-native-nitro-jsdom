import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM typed events', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('KeyboardEvent exposes key/code and modifier flags', async () => {
    dom = JSDOM.create('<html><body><input id="i"></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      let captured;
      input.addEventListener('keydown', (e) => {
        captured = { key: e.key, code: e.code, ctrlKey: e.ctrlKey, type: e.type };
      });
      input.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', code: 'Enter', ctrlKey: true, bubbles: true }));
      JSON.stringify(captured);
    `);
    expect(JSON.parse(result)).toEqual({ key: 'Enter', code: 'Enter', ctrlKey: true, type: 'keydown' });
  });

  it('MouseEvent exposes clientX/clientY and button', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      let captured;
      div.addEventListener('click', (e) => {
        captured = { clientX: e.clientX, clientY: e.clientY, button: e.button };
      });
      div.dispatchEvent(new MouseEvent('click', { clientX: 12, clientY: 34, button: 0, bubbles: true }));
      JSON.stringify(captured);
    `);
    expect(JSON.parse(result)).toEqual({ clientX: 12, clientY: 34, button: 0 });
  });

  it('typed events default modifier/coordinate fields when no init dict is given', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const e = new KeyboardEvent('keyup');
      JSON.stringify({ key: e.key, ctrlKey: e.ctrlKey, bubbles: e.bubbles });
    `);
    expect(JSON.parse(result)).toEqual({ key: '', ctrlKey: false, bubbles: false });
  });
});
