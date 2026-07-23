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

  it('File extends Blob with name/lastModified and inherits text()', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const file = new File(['hello'], 'greeting.txt', { type: 'text/plain', lastModified: 1700000000000 });
      file.text().then((text) => JSON.stringify({
        name: file.name,
        size: file.size,
        type: file.type,
        lastModified: file.lastModified,
        isBlob: file instanceof Blob,
        text,
      }));
    `);
    expect(JSON.parse(result)).toEqual({
      name: 'greeting.txt',
      size: 5,
      type: 'text/plain',
      lastModified: 1700000000000,
      isBlob: true,
      text: 'hello',
    });
  });

  it('File defaults lastModified to the current time when not provided', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const before = Date.now();
      const file = new File(['x'], 'a.txt');
      const after = Date.now();
      JSON.stringify(file.lastModified >= before && file.lastModified <= after);
    `);
    expect(JSON.parse(result)).toBe(true);
  });

  it('File coerces a non-number lastModified per Web IDL long long semantics', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify({
        fromString: new File(['x'], 'a.txt', { lastModified: '123' }).lastModified,
        fromFloat: new File(['x'], 'a.txt', { lastModified: 123.9 }).lastModified,
        fromNaN: new File(['x'], 'a.txt', { lastModified: NaN }).lastModified,
        fromInfinity: new File(['x'], 'a.txt', { lastModified: Infinity }).lastModified,
        fromNull: new File(['x'], 'a.txt', { lastModified: null }).lastModified,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      fromString: 123,
      fromFloat: 123,
      fromNaN: 0,
      fromInfinity: 0,
      fromNull: 0,
    });
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
