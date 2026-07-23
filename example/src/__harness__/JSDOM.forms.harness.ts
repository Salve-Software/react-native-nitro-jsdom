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

  it('validity.valueMissing is true for an empty required field and false once filled', async () => {
    dom = JSDOM.create('<html><body><input id="i" required></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      const before = { valueMissing: input.validity.valueMissing, valid: input.validity.valid };
      input.value = 'hi';
      const after = { valueMissing: input.validity.valueMissing, valid: input.validity.valid };
      JSON.stringify({ before, after });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { valueMissing: true, valid: false },
      after: { valueMissing: false, valid: true },
    });
  });

  it('validity.valueMissing considers a required checkbox unchecked and a required select with no value', async () => {
    dom = JSDOM.create(`
      <html><body>
        <input id="c" type="checkbox" required>
        <select id="s" required><option value="">Choose</option><option value="a">A</option></select>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const checkbox = document.getElementById('c');
      const select = document.getElementById('s');
      const before = { checkbox: checkbox.validity.valueMissing, select: select.validity.valueMissing };
      checkbox.checked = true;
      select.value = 'a';
      const after = { checkbox: checkbox.validity.valueMissing, select: select.validity.valueMissing };
      JSON.stringify({ before, after });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { checkbox: true, select: true },
      after: { checkbox: false, select: false },
    });
  });

  it('validity.typeMismatch flags invalid email/url values and clears on a valid one', async () => {
    dom = JSDOM.create('<html><body><input id="e" type="email"><input id="u" type="url"></body></html>');
    const result = await dom.evaluate(`
      const email = document.getElementById('e');
      const url = document.getElementById('u');
      email.value = 'not-an-email';
      url.value = 'not a url';
      const bad = { email: email.validity.typeMismatch, url: url.validity.typeMismatch };
      email.value = 'a@b.com';
      url.value = 'https://example.com';
      const good = { email: email.validity.typeMismatch, url: url.validity.typeMismatch };
      JSON.stringify({ bad, good });
    `);
    expect(JSON.parse(result)).toEqual({
      bad: { email: true, url: true },
      good: { email: false, url: false },
    });
  });

  it('validity.patternMismatch checks the pattern attribute against the value', async () => {
    dom = JSDOM.create('<html><body><input id="i" pattern="[0-9]{3}"></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      input.value = 'abc';
      const bad = input.validity.patternMismatch;
      input.value = '123';
      const good = input.validity.patternMismatch;
      JSON.stringify({ bad, good });
    `);
    expect(JSON.parse(result)).toEqual({ bad: true, good: false });
  });

  it('validity.tooLong/tooShort respect maxlength/minlength', async () => {
    dom = JSDOM.create('<html><body><input id="i" minlength="3" maxlength="5"></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      input.value = 'ab';
      const tooShort = input.validity.tooShort;
      input.value = 'abcdef';
      const tooLong = input.validity.tooLong;
      input.value = 'abcd';
      const ok = input.validity.valid;
      JSON.stringify({ tooShort, tooLong, ok });
    `);
    expect(JSON.parse(result)).toEqual({ tooShort: true, tooLong: true, ok: true });
  });

  it('validity.rangeUnderflow/rangeOverflow/badInput apply to number inputs', async () => {
    dom = JSDOM.create('<html><body><input id="i" type="number" min="1" max="10"></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      input.value = '0';
      const under = input.validity.rangeUnderflow;
      input.value = '20';
      const over = input.validity.rangeOverflow;
      input.value = 'nope';
      const badInput = input.validity.badInput;
      input.value = '5';
      const ok = input.validity.valid;
      JSON.stringify({ under, over, badInput, ok });
    `);
    expect(JSON.parse(result)).toEqual({ under: true, over: true, badInput: true, ok: true });
  });

  it('setCustomValidity() forces customError/invalid until cleared with an empty string', async () => {
    dom = JSDOM.create('<html><body><input id="i" value="anything"></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      input.setCustomValidity('nope');
      const invalid = { customError: input.validity.customError, valid: input.validity.valid, message: input.validationMessage };
      input.setCustomValidity('');
      const valid = { customError: input.validity.customError, valid: input.validity.valid, message: input.validationMessage };
      JSON.stringify({ invalid, valid });
    `);
    expect(JSON.parse(result)).toEqual({
      invalid: { customError: true, valid: false, message: 'nope' },
      valid: { customError: false, valid: true, message: '' },
    });
  });

  it('checkValidity() fires a cancelable "invalid" event and returns the validity boolean', async () => {
    dom = JSDOM.create('<html><body><input id="i" required></body></html>');
    const result = await dom.evaluate(`
      const input = document.getElementById('i');
      let invalidFired = false;
      let wasCancelable = false;
      input.addEventListener('invalid', (e) => {
        invalidFired = true;
        wasCancelable = e.cancelable;
      });
      const invalidResult = input.checkValidity();
      input.value = 'hi';
      const validResult = input.checkValidity();
      JSON.stringify({ invalidFired, wasCancelable, invalidResult, validResult });
    `);
    expect(JSON.parse(result)).toEqual({
      invalidFired: true,
      wasCancelable: true,
      invalidResult: false,
      validResult: true,
    });
  });

  it('reportValidity() behaves the same as checkValidity() (no UI layer to report against)', async () => {
    dom = JSDOM.create('<html><body><input id="i" required></body></html>');
    const result = await dom.evaluate(`document.getElementById('i').reportValidity()`);
    expect(result).toBe('false');
  });

  it('form.checkValidity() checks every non-disabled field and skips disabled ones', async () => {
    dom = JSDOM.create(`
      <html><body>
        <form id="f">
          <input name="a" required>
          <input name="b" required disabled>
        </form>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const form = document.getElementById('f');
      const before = form.checkValidity();
      form.querySelector('[name="a"]').value = 'hi';
      const after = form.checkValidity();
      JSON.stringify({ before, after });
    `);
    expect(JSON.parse(result)).toEqual({ before: false, after: true });
  });

  it('willValidate is false for disabled or hidden fields and true for a normal input', async () => {
    dom = JSDOM.create(`
      <html><body>
        <input id="normal">
        <input id="disabled" disabled>
        <input id="hidden" type="hidden">
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify({
        normal: document.getElementById('normal').willValidate,
        disabled: document.getElementById('disabled').willValidate,
        hidden: document.getElementById('hidden').willValidate,
      });
    `);
    expect(JSON.parse(result)).toEqual({ normal: true, disabled: false, hidden: false });
  });

  it('validity getter returns a real ValidityState instance', async () => {
    dom = JSDOM.create('<html><body><input id="i"></body></html>');
    const result = await dom.evaluate(`
      String(document.getElementById('i').validity instanceof ValidityState);
    `);
    expect(result).toBe('true');
  });

  it('element.form resolves via ancestor <form>, or the form= attribute, and is undefined for non-form-associated elements', async () => {
    dom = JSDOM.create(`
      <html><body>
        <form id="f1"><input id="nested"></form>
        <form id="f2"></form>
        <input id="remote" form="f2">
        <div id="plain"></div>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify({
        nested: document.getElementById('nested').form.id,
        remote: document.getElementById('remote').form.id,
        plain: document.getElementById('plain').form === undefined,
      });
    `);
    expect(JSON.parse(result)).toEqual({ nested: 'f1', remote: 'f2', plain: true });
  });

  it('element.form is null when a form-associated element has no owning form', async () => {
    dom = JSDOM.create('<html><body><input id="orphan"></body></html>');
    const result = await dom.evaluate('document.getElementById("orphan").form === null');
    expect(result).toBe('true');
  });

  it('form.elements lists all form-associated descendants, including disabled ones', async () => {
    dom = JSDOM.create(`
      <html><body>
        <form id="f">
          <input id="a">
          <input id="b" disabled>
          <select id="c"></select>
          <button id="d"></button>
        </form>
      </body></html>
    `);
    const result = await dom.evaluate(`
      JSON.stringify(document.getElementById('f').elements.map((el) => el.id));
    `);
    expect(JSON.parse(result)).toEqual(['a', 'b', 'c', 'd']);
  });
});
