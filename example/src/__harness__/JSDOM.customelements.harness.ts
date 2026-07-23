import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM Custom Elements', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('define()/get() register and look up a constructor', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      class MyEl extends HTMLElement {}
      customElements.define('my-el', MyEl);
      JSON.stringify({
        getReturnsCtor: customElements.get('my-el') === MyEl,
        getUndefinedForUnknown: customElements.get('unknown-el') === undefined,
      });
    `);
    expect(JSON.parse(result)).toEqual({
      getReturnsCtor: true,
      getUndefinedForUnknown: true,
    });
  });

  it('document.createElement() upgrades the prototype but does not connect it', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class UpgradeEl extends HTMLElement {
        connectedCallback() { log.push('connected'); }
      }
      customElements.define('upgrade-el', UpgradeEl);
      const el = document.createElement('upgrade-el');
      JSON.stringify({ isInstance: el instanceof UpgradeEl, log });
    `);
    expect(JSON.parse(result)).toEqual({ isInstance: true, log: [] });
  });

  it('appendChild()/removeChild() fire connectedCallback/disconnectedCallback', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class LifecycleEl extends HTMLElement {
        connectedCallback() { log.push('connected'); }
        disconnectedCallback() { log.push('disconnected'); }
      }
      customElements.define('lifecycle-el', LifecycleEl);
      const el = document.createElement('lifecycle-el');

      document.body.appendChild(el);
      const afterAppend = log.slice();

      document.body.removeChild(el);
      const afterRemove = log.slice();

      JSON.stringify({ afterAppend, afterRemove });
    `);
    expect(JSON.parse(result)).toEqual({
      afterAppend: ['connected'],
      afterRemove: ['connected', 'disconnected'],
    });
  });

  it('appendChild() with a DocumentFragment connects custom elements moved out of it', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class FragEl extends HTMLElement {
        connectedCallback() { log.push('connected'); }
      }
      customElements.define('frag-el', FragEl);

      const frag = document.createDocumentFragment();
      const el = document.createElement('frag-el');
      frag.appendChild(el);
      const beforeAppend = log.slice();

      document.body.appendChild(frag);
      JSON.stringify({ beforeAppend, afterAppend: log });
    `);
    expect(JSON.parse(result)).toEqual({ beforeAppend: [], afterAppend: ['connected'] });
  });

  it('innerHTML assignment disconnects old custom-element content before connecting the new content', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class ReplaceEl extends HTMLElement {
        connectedCallback() { log.push('connected:' + this.id); }
        disconnectedCallback() { log.push('disconnected:' + this.id); }
      }
      customElements.define('replace-el', ReplaceEl);
      const host = document.getElementById('host');
      host.innerHTML = '<replace-el id="old"></replace-el>';
      const afterFirstInsert = log.slice();

      host.innerHTML = '<replace-el id="new"></replace-el>';
      JSON.stringify({ afterFirstInsert, afterReplace: log });
    `);
    expect(JSON.parse(result)).toEqual({
      afterFirstInsert: ['connected:old'],
      afterReplace: ['connected:old', 'disconnected:old', 'connected:new'],
    });
  });

  it('attributeChangedCallback only fires for attributes in observedAttributes', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class ObservedEl extends HTMLElement {
        static get observedAttributes() { return ['foo']; }
        attributeChangedCallback(name, oldValue, newValue) {
          log.push(name + ':' + oldValue + '->' + newValue);
        }
      }
      customElements.define('observed-el', ObservedEl);
      const el = document.createElement('observed-el');
      el.setAttribute('foo', 'bar');
      el.setAttribute('baz', 'qux');
      el.removeAttribute('foo');
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['foo:null->bar', 'foo:bar->null']);
  });

  it('upgrading an element with a matching attribute already set fires attributeChangedCallback once', async () => {
    dom = JSDOM.create('<html><body><preset-el foo="initial"></preset-el></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class PresetEl extends HTMLElement {
        static get observedAttributes() { return ['foo']; }
        attributeChangedCallback(name, oldValue, newValue) {
          log.push(name + ':' + oldValue + '->' + newValue);
        }
      }
      customElements.define('preset-el', PresetEl);
      JSON.stringify(log);
    `);
    expect(JSON.parse(result)).toEqual(['foo:null->initial']);
  });

  it('define() rescans the document, upgrading and connecting elements already in the initial HTML', async () => {
    dom = JSDOM.create('<html><body><existing-el></existing-el></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class ExistingEl extends HTMLElement {
        connectedCallback() { log.push('connected'); }
      }
      customElements.define('existing-el', ExistingEl);
      const el = document.querySelector('existing-el');
      JSON.stringify({ isInstance: el instanceof ExistingEl, log });
    `);
    expect(JSON.parse(result)).toEqual({ isInstance: true, log: ['connected'] });
  });

  it('Element.prototype.innerHTML assignment upgrades newly parsed descendants', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      class InnerEl extends HTMLElement {}
      customElements.define('inner-el', InnerEl);
      document.getElementById('host').innerHTML = '<inner-el></inner-el>';
      JSON.stringify({
        isInstance: document.querySelector('inner-el') instanceof InnerEl,
      });
    `);
    expect(JSON.parse(result)).toEqual({ isInstance: true });
  });

  it('ShadowRoot.prototype.innerHTML assignment upgrades descendants inside the shadow tree', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      class ShadowEl extends HTMLElement {}
      customElements.define('shadow-el', ShadowEl);
      const shadow = document.getElementById('host').attachShadow({ mode: 'open' });
      shadow.innerHTML = '<shadow-el></shadow-el>';
      JSON.stringify({
        isInstance: shadow.querySelector('shadow-el') instanceof ShadowEl,
      });
    `);
    expect(JSON.parse(result)).toEqual({ isInstance: true });
  });

  it('appendChild()/removeChild() of a host element also connects/disconnects custom elements inside its (closed) shadow root', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class NestedEl extends HTMLElement {
        connectedCallback() { log.push('connected'); }
        disconnectedCallback() { log.push('disconnected'); }
      }
      customElements.define('nested-el', NestedEl);

      const host = document.createElement('div');
      const shadow = host.attachShadow({ mode: 'closed' });
      const nested = document.createElement('nested-el');
      shadow.appendChild(nested);
      const beforeHostConnect = log.slice();

      document.body.appendChild(host);
      const afterHostConnect = log.slice();

      document.body.removeChild(host);
      JSON.stringify({ beforeHostConnect, afterHostConnect, afterHostDisconnect: log });
    `);
    expect(JSON.parse(result)).toEqual({
      beforeHostConnect: [],
      afterHostConnect: ['connected'],
      afterHostDisconnect: ['connected', 'disconnected'],
    });
  });

  it('insertAdjacentHTML() upgrades the newly inserted elements', async () => {
    dom = JSDOM.create('<html><body><div id="host"></div></body></html>');
    const result = await dom.evaluate(`
      class AdjEl extends HTMLElement {}
      customElements.define('adj-el', AdjEl);
      document.getElementById('host').insertAdjacentHTML('beforeend', '<adj-el></adj-el>');
      JSON.stringify({
        isInstance: document.querySelector('adj-el') instanceof AdjEl,
      });
    `);
    expect(JSON.parse(result)).toEqual({ isInstance: true });
  });

  it('whenDefined() resolves immediately for an already-defined name and later for a pending one', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      (async () => {
        const order = [];
        customElements.define('now-el', class extends HTMLElement {});
        await customElements.whenDefined('now-el');
        order.push('now-resolved');

        const pending = customElements.whenDefined('later-el').then(() => order.push('later-resolved'));
        order.push('define-not-yet-called');
        customElements.define('later-el', class extends HTMLElement {});
        await pending;

        return JSON.stringify(order);
      })()
    `);
    expect(JSON.parse(result)).toEqual([
      'now-resolved',
      'define-not-yet-called',
      'later-resolved',
    ]);
  });

  it('define() with an invalid name throws a SyntaxError DOMException', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      let caught;
      try {
        customElements.define('NoHyphen', class extends HTMLElement {});
      } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'SyntaxError', isDOMException: true });
  });

  it('define() with an already-registered name throws a NotSupportedError DOMException', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      customElements.define('dup-el', class extends HTMLElement {});
      let caught;
      try {
        customElements.define('dup-el', class extends HTMLElement {});
      } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'NotSupportedError', isDOMException: true });
  });

  it('customElements.upgrade(root) upgrades elements a shadow-crossing define() rescan missed', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      const log = [];
      class LateEl extends HTMLElement {
        connectedCallback() { log.push('connected'); }
      }
      const host = document.createElement('div');
      const shadow = host.attachShadow({ mode: 'open' });
      shadow.innerHTML = '<late-el></late-el>';
      document.body.appendChild(host);

      // define() happens after the shadow content already exists — its own
      // document.querySelectorAll(name) rescan can't see inside the shadow tree.
      customElements.define('late-el', LateEl);
      const beforeUpgrade = shadow.querySelector('late-el') instanceof LateEl;

      customElements.upgrade(shadow);
      const afterUpgrade = shadow.querySelector('late-el') instanceof LateEl;

      JSON.stringify({ beforeUpgrade, afterUpgrade, log });
    `);
    expect(JSON.parse(result)).toEqual({
      beforeUpgrade: false,
      afterUpgrade: true,
      log: ['connected'],
    });
  });

  it('customElements.upgrade(root) upgrades a detached root itself, not just its descendants', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      class InclusiveEl extends HTMLElement {}
      // Created and left detached before define() — define()'s own rescan
      // (document.querySelectorAll) can't see a node outside the document tree.
      const real = document.createElement('inclusive-el');
      customElements.define('inclusive-el', InclusiveEl);
      const before = real instanceof InclusiveEl;
      customElements.upgrade(real);
      const after = real instanceof InclusiveEl;
      JSON.stringify({ before, after });
    `);
    expect(JSON.parse(result)).toEqual({ before: false, after: true });
  });

  it('customElements.upgrade() with a non-Node argument throws a TypeError', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      function attempt(value) {
        try { customElements.upgrade(value); return null; } catch (e) { return e.constructor.name; }
      }
      JSON.stringify({
        nullResult: attempt(null),
        undefinedResult: attempt(undefined),
        fakeNodeResult: attempt({ nodeType: 1 }),
        stringResult: attempt('div'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      nullResult: 'TypeError',
      undefinedResult: 'TypeError',
      fakeNodeResult: 'TypeError',
      stringResult: 'TypeError',
    });
  });

  it('define() with a constructor already used for another name throws a NotSupportedError DOMException', async () => {
    dom = JSDOM.create('<html><body></body></html>');
    const result = await dom.evaluate(`
      class SharedEl extends HTMLElement {}
      customElements.define('shared-el-one', SharedEl);
      let caught;
      try {
        customElements.define('shared-el-two', SharedEl);
      } catch (e) {
        caught = { name: e.name, isDOMException: e instanceof DOMException };
      }
      JSON.stringify(caught);
    `);
    expect(JSON.parse(result)).toEqual({ name: 'NotSupportedError', isDOMException: true });
  });
});
