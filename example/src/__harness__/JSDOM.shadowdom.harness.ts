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

  it('assignedNodes()/assignedElements() route light-DOM children to matching named/default slots', async () => {
    dom = JSDOM.create(
      '<html><body><div id="host"><span slot="title">Title</span>text-in-default-slot<p>also-default</p></div></body></html>'
    );
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<slot name="title"></slot><slot></slot>';
      const titleSlot = shadow.querySelector('slot[name="title"]');
      const defaultSlot = shadow.querySelector('slot:not([name])');
      JSON.stringify({
        titleAssigned: titleSlot.assignedElements().map((el) => el.textContent),
        defaultAssignedElements: defaultSlot.assignedElements().map((el) => el.tagName),
        defaultAssignedNodesLength: defaultSlot.assignedNodes().length,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      titleAssigned: ['Title'],
      defaultAssignedElements: ['P'],
      defaultAssignedNodesLength: 2, // the bare text node + <p>
    });
  });

  it('assignedNodes({flatten: true}) falls back to the slot\'s own fallback content when nothing is assigned', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<slot name="empty"><em>fallback</em></slot>';
      const slot = shadow.querySelector('slot');
      JSON.stringify({
        withoutFlatten: slot.assignedNodes().length,
        withFlatten: slot.assignedNodes({ flatten: true }).map((n) => n.tagName),
      });
    `);
    expect(JSON.parse(result)).toEqual({ withoutFlatten: 0, withFlatten: ['EM'] });
  });

  it('assignedNodes()/assignedElements() return [] for non-<slot> elements', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      JSON.stringify({ nodes: div.assignedNodes(), elements: div.assignedElements() });
    `);
    expect(JSON.parse(result)).toEqual({ nodes: [], elements: [] });
  });

  it('slotchange fires when a light-DOM child is appended/removed on the host', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<slot></slot>';
      const slot = shadow.querySelector('slot');
      const log = [];
      slot.addEventListener('slotchange', () => log.push(slot.assignedNodes().length));

      const child = document.createElement('span');
      host.appendChild(child);
      const afterAppend = log.slice();

      host.removeChild(child);
      JSON.stringify({ afterAppend, afterRemove: log });
    `);
    expect(JSON.parse(result)).toEqual({ afterAppend: [1], afterRemove: [1, 0] });
  });

  it('slotchange fires when a light-DOM child\'s slot attribute changes', async () => {
    dom = JSDOM.create(`
      <html><body>
        <div id="host"><span id="c">hi</span></div>
      </body></html>
    `);
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<slot name="a"></slot><slot name="b"></slot>';
      const slotA = shadow.querySelector('slot[name="a"]');
      const slotB = shadow.querySelector('slot[name="b"]');
      const log = [];
      slotA.addEventListener('slotchange', () => log.push('a'));
      slotB.addEventListener('slotchange', () => log.push('b'));

      const child = document.getElementById('c');
      child.setAttribute('slot', 'a');
      const afterFirst = log.slice();

      child.setAttribute('slot', 'b');
      JSON.stringify({ afterFirst, afterSecond: log });
    `);
    // Moving the child from slot "a" to slot "b" fires slotchange on both:
    // "a" loses its assignment (1 -> 0), "b" gains one (0 -> 1).
    expect(JSON.parse(result)).toEqual({ afterFirst: ['a'], afterSecond: ['a', 'a', 'b'] });
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

  it('getRootNode() stops at the ShadowRoot by default, but crosses into the light DOM with {composed: true}', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const host = document.getElementById('host');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<span id="inner">hi</span>';
      const inner = shadow.querySelector('#inner');
      JSON.stringify({
        defaultRootIsShadow: inner.getRootNode() === shadow,
        composedRootIsDocument: inner.getRootNode({ composed: true }) === document,
      });
    `);
    expect(JSON.parse(result)).toEqual({ defaultRootIsShadow: true, composedRootIsDocument: true });
  });
});
