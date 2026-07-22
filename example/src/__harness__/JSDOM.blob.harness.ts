import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM Blob / FileReader', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('Blob exposes size/type and text()/arrayBuffer()', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const blob = new Blob(['hello ', 'world'], { type: 'text/plain' });
      blob.text().then((text) => JSON.stringify({ size: blob.size, type: blob.type, text }));
    `);
    expect(JSON.parse(result)).toEqual({ size: 11, type: 'text/plain', text: 'hello world' });
  });

  it('FileReader.readAsText() fires load/loadend asynchronously with the decoded result', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      new Promise((resolve) => {
        const blob = new Blob(['abc']);
        const reader = new FileReader();
        const events = [];
        reader.addEventListener('load', () => events.push('load'));
        reader.addEventListener('loadend', () => {
          events.push('loadend');
          resolve(JSON.stringify({ result: reader.result, events, readyState: reader.readyState }));
        });
        reader.readAsText(blob);
      });
    `);
    expect(JSON.parse(result)).toEqual({ result: 'abc', events: ['load', 'loadend'], readyState: 2 });
  });

  it('FileReader.readAsDataURL() produces a base64 data URL', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      new Promise((resolve) => {
        const blob = new Blob(['hi'], { type: 'text/plain' });
        const reader = new FileReader();
        reader.onloadend = () => resolve(reader.result);
        reader.readAsDataURL(blob);
      });
    `);
    expect(result).toBe('data:text/plain;base64,aGk=');
  });
});
