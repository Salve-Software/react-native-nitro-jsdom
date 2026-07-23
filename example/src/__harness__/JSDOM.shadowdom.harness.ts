import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM Shadow DOM', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('attachShadow({mode: "open"}) returns a ShadowRoot with host/mode set, cached across accesses', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      JSON.stringify({
        isShadowRootInstance: shadow instanceof ShadowRoot,
        mode: shadow.mode,
        hostIsSameElement: shadow.host === host,
        shadowRootGetterSameObject: host.shadowRoot === shadow,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      isShadowRootInstance: true,
      mode: 'open',
      hostIsSameElement: true,
      shadowRootGetterSameObject: true,
    });
  });

  it('closed mode hides shadowRoot from the getter but the attachShadow() return value still works', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'closed' });
      shadow.innerHTML = '<span>hi</span>';
      JSON.stringify({
        getterIsNull: host.shadowRoot === null,
        shadowStillUsable: shadow.querySelector('span').textContent,
      });
    `);
    expect(JSON.parse(result)).toEqual({ getterIsNull: true, shadowStillUsable: 'hi' });
  });

  it('shadow tree is scoped: querySelector/children inside it are separate from the light DOM', async () => {
    dom = JSDOM.create('<html><body><div id="host"><p>light</p></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<p>shadow</p>';
      JSON.stringify({
        hostChildrenUnchanged: host.children.length,
        hostFirstChildText: host.children[0].textContent,
        shadowQuery: shadow.querySelector('p').textContent,
        shadowQueryAllLength: shadow.querySelectorAll('p').length,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      hostChildrenUnchanged: 1,
      hostFirstChildText: 'light',
      shadowQuery: 'shadow',
      shadowQueryAllLength: 1,
    });
  });

  it('appendChild/childNodes work on the shadow root via inherited Node methods', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      const span = document.createElement('span');
      span.textContent = 'appended';
      shadow.appendChild(span);
      JSON.stringify({
        childNodesLength: shadow.childNodes.length,
        innerHTML: shadow.innerHTML,
      });
    `);
    expect(JSON.parse(result)).toEqual({ childNodesLength: 1, innerHTML: '<span>appended</span>' });
  });

  it('attachShadow() twice on the same element throws NotSupportedError DOMException', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      host.attachShadow({ mode: 'open' });
      let caught;
      try { host.attachShadow({ mode: 'open' }); } catch (e) { caught = { name: e.name, isDOMException: e instanceof DOMException }; }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'NotSupportedError', isDOMException: true });
  });

  it('attachShadow() with a missing/invalid mode throws TypeError', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      let caught;
      try { host.attachShadow({}); } catch (e) { caught = e.constructor.name; }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toBe('TypeError');
  });

  it('attachShadow() propagates the real exception when the mode getter throws, instead of masking it', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      let caught;
      try {
        host.attachShadow({ get mode() { throw new RangeError('boom'); } });
      } catch (e) {
        caught = { name: e.constructor.name, message: e.message };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'RangeError', message: 'boom' });
  });
});
