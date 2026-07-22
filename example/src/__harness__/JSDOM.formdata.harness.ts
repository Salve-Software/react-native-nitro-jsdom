import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM FormData / form submit', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('FormData built from scratch supports append/get/getAll/has/delete', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const fd = new FormData();
      fd.append('tag', 'a');
      fd.append('tag', 'b');
      fd.set('name', 'ana');
      const before = { tags: fd.getAll('tag'), name: fd.get('name'), hasName: fd.has('name') };
      fd.delete('tag');
      JSON.stringify({ before, afterDelete: fd.getAll('tag') });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { tags: ['a', 'b'], name: 'ana', hasName: true },
      afterDelete: [],
    });
  });

  it('FormData(form) collects named form controls, skipping unchecked checkboxes', async () => {
    dom = JSDOM.create(`
      <html><body>
        <form id="f">
          <input name="username" value="angelo">
          <input name="subscribe" type="checkbox" checked>
          <input name="marketing" type="checkbox">
          <input value="no-name">
        </form>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const form = document.getElementById('f');
      const fd = new FormData(form);
      JSON.stringify(Array.from(fd.entries()));
    `);
    expect(JSON.parse(result)).toEqual([
      ['username', 'angelo'],
      ['subscribe', 'on'],
    ]);
  });

  it('requestSubmit() dispatches a cancelable submit event with a submitter', async () => {
    dom = JSDOM.create('<html><body><form id="f"></form></body></html>');
    const result = await dom.evaluate(`
      const form = document.getElementById('f');
      let captured;
      form.addEventListener('submit', (e) => {
        captured = { type: e.type, bubbles: e.bubbles, cancelable: e.cancelable, submitter: e.submitter };
        e.preventDefault();
      });
      const returnValue = form.requestSubmit('the-button');
      JSON.stringify({ captured, defaultPreventedAfter: undefined });
    `);
    expect(JSON.parse(result).captured).toEqual({
      type: 'submit',
      bubbles: true,
      cancelable: true,
      submitter: 'the-button',
    });
  });

  it('submit() does not fire a submit event', async () => {
    dom = JSDOM.create('<html><body><form id="f"></form></body></html>');
    const result = await dom.evaluate(`
      const form = document.getElementById('f');
      let fired = false;
      form.addEventListener('submit', () => { fired = true; });
      form.submit();
      String(fired);
    `);
    expect(result).toBe('false');
  });
});
