import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM form elements', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('input.value reads and writes the value attribute', async () => {
    dom = JSDOM.create('<html><body><input id="i" value="hi"></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      const before = input.value;
      input.value = 'updated';
      JSON.stringify({ before, after: input.value, attr: input.getAttribute('value') });
    `);
    expect(JSON.parse(result)).toEqual({ before: 'hi', after: 'updated', attr: 'updated' });
  });

  it('textarea.value reads and writes its text content', async () => {
    dom = JSDOM.create('<html><body><textarea id="t">hello</textarea></body></html>');
    const result = await dom.evaluate(`
      const textarea = document.getElementById('t');
      const before = textarea.value;
      textarea.value = 'changed';
      JSON.stringify({ before, after: textarea.value, textContent: textarea.textContent });
    `);
    expect(JSON.parse(result)).toEqual({ before: 'hello', after: 'changed', textContent: 'changed' });
  });

  it('checkbox.checked reflects and toggles the checked attribute', async () => {
    dom = JSDOM.create('<html><body><input id="c" type="checkbox"></body></html>');
    const result = await dom.evaluate(`
      const checkbox = document.getElementById('c');
      const before = checkbox.checked;
      checkbox.checked = true;
      const afterCheck = { checked: checkbox.checked, attr: checkbox.getAttribute('checked') };
      checkbox.checked = false;
      const afterUncheck = { checked: checkbox.checked, attr: checkbox.getAttribute('checked') };
      JSON.stringify({ before, afterCheck, afterUncheck });
    `);
    expect(JSON.parse(result)).toEqual({
      before: false,
      afterCheck: { checked: true, attr: '' },
      afterUncheck: { checked: false, attr: null },
    });
  });

  it('checkbox.checked starts true when the checked attribute is present in markup', async () => {
    dom = JSDOM.create('<html><body><input id="c" type="checkbox" checked></body></html>');
    const result = await dom.evaluate(`document.getElementById('c').checked`);
    expect(result).toBe('true');
  });

  it('select.value reads the selected option, defaulting to the first option', async () => {
    dom = JSDOM.create(`
      <html><body>
        <select id="s">
          <option value="a">A</option>
          <option value="b" selected>B</option>
          <option value="c">C</option>
        </select>
      </body></html>
    `);
    const result = await dom.evaluate(`document.getElementById('s').value`);
    expect(result).toBe('b');
  });

  it('select.value defaults to the first option when none is marked selected', async () => {
    dom = JSDOM.create(`
      <html><body>
        <select id="s">
          <option value="a">A</option>
          <option value="b">B</option>
        </select>
      </body></html>
    `);
    const result = await dom.evaluate(`document.getElementById('s').value`);
    expect(result).toBe('a');
  });

  it('select.value falls back to option text content when no value attribute is set', async () => {
    dom = JSDOM.create(`
      <html><body>
        <select id="s">
          <option>Alpha</option>
          <option selected>Beta</option>
        </select>
      </body></html>
    `);
    const result = await dom.evaluate(`document.getElementById('s').value`);
    expect(result).toBe('Beta');
  });

  it('setting select.value updates which option is selected', async () => {
    dom = JSDOM.create(`
      <html><body>
        <select id="s">
          <option value="a">A</option>
          <option value="b" selected>B</option>
          <option value="c">C</option>
        </select>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const select = document.getElementById('s');
      select.value = 'c';
      const options = Array.from(document.querySelectorAll('option'));
      JSON.stringify({
        value: select.value,
        selectedFlags: options.map((o) => o.hasAttribute('selected')),
      });
    `);
    expect(JSON.parse(result)).toEqual({ value: 'c', selectedFlags: [false, false, true] });
  });
});
