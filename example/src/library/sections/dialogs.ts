import type { ISection } from '../../types';
import { Alert } from 'react-native';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const dialogsSection = async (): Promise<ISection> => {
  const html = `<html><body></body></html>`;
  const results = [];

  // alert — automated
  {
    let captured = '';
    const dom = JSDOM.create(html, { onAlert: (m) => { captured = m; } });
    const res = await dom.evaluate(`window.alert('hello')`);
    results.push({ label: 'alert with callback — captured — expect: hello', value: captured });
    results.push({ label: 'alert returns undefined — expect: undefined', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    let ok = true;
    let res = '';
    try { res = await dom.evaluate(`window.alert('hello'); 1`); } catch { ok = false; }
    results.push({ label: 'alert without callback — no throw — expect: true', value: String(ok) });
    results.push({ label: 'alert without callback — returns 1 — expect: 1', value: res });
    dom.dispose();
  }

  // confirm — automated
  {
    const dom = JSDOM.create(html, { onConfirm: () => true });
    const res = await dom.evaluate(`window.confirm('ok?')`);
    results.push({ label: 'confirm with callback true — expect: true', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { onConfirm: () => false });
    const res = await dom.evaluate(`window.confirm('ok?')`);
    results.push({ label: 'confirm with callback false — expect: false', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`window.confirm('ok?')`);
    results.push({ label: 'confirm without callback — expect: false', value: res });
    dom.dispose();
  }

  // prompt — automated
  {
    const dom = JSDOM.create(html, { onPrompt: (_m, d) => (d ?? '') + '!' });
    const res = await dom.evaluate(`window.prompt('name?', 'default')`);
    results.push({ label: 'prompt with callback — expect: default!', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { onPrompt: () => null });
    const res = await dom.evaluate(`window.prompt('x')`);
    results.push({ label: 'prompt callback returning null — expect: null', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html);
    const res = await dom.evaluate(`window.prompt('x')`);
    results.push({ label: 'prompt without callback — expect: null', value: res });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { onPrompt: (_m, d) => d === undefined ? '<none>' : d });
    const resAbsent = await dom.evaluate(`window.prompt('x')`);
    const resPresent = await dom.evaluate(`window.prompt('x', 'abc')`);
    results.push({ label: 'prompt defaultValue absent — expect: <none>', value: resAbsent });
    results.push({ label: 'prompt defaultValue present — expect: abc', value: resPresent });
    dom.dispose();
  }

  {
    const dom = JSDOM.create(html, { onPrompt: (_m, d) => d === undefined ? '<absent>' : `<present:${d}>` });
    const resNull = await dom.evaluate(`window.prompt('x', null)`);
    results.push({ label: 'prompt null coercion — expect: <present:null>', value: resNull });
    dom.dispose();
  }

  {
    const order: string[] = [];
    const dom = JSDOM.create(html, { onAlert: () => order.push('alert') });
    await dom.evaluate(`window.alert('x')`);
    results.push({ label: 'alert sync ordering — expect: alert', value: order[0] ?? 'not-fired' });
    dom.dispose();
  }

  // interactive
  results.push({
    label: 'window.alert("Hello!") — press ▶ to trigger',
    value: 'tap to run',
    onPress: async () => {
      const dom = JSDOM.create(html, {
        onAlert: (msg) => Alert.alert('Alert (from sandbox)', msg),
      });
      await dom.evaluate(`window.alert("Hello from sandbox!")`);
      dom.dispose();
      return '✓ fired — check the RN alert above';
    },
  });

  results.push({
    label: 'window.confirm("Continue?") — press ▶',
    value: 'tap to run',
    onPress: async () => {
      const dom = JSDOM.create(html, {
        onConfirm: (msg) => new Promise<boolean>((resolve) => {
          Alert.alert('Confirm (from sandbox)', msg, [
            { text: 'Cancel', style: 'cancel', onPress: () => resolve(false) },
            { text: 'OK', onPress: () => resolve(true) },
          ]);
        }),
      });
      const raw = await dom.evaluate(`String(window.confirm("Continue?"))`);
      dom.dispose();
      return `✓ confirm returned: ${raw}`;
    },
  });

  results.push({
    label: 'window.prompt("Your name?") — press ▶',
    value: 'tap to run',
    onPress: async () => {
      const dom = JSDOM.create(html, { onPrompt: (_msg, def) => `Hello, ${def ?? 'stranger'}!` });
      const res = await dom.evaluate(`window.prompt("Your name?", "World")`);
      dom.dispose();
      return `✓ prompt returned: "${res}"`;
    },
  });

  return { title: 'Dialogs', results };
};
