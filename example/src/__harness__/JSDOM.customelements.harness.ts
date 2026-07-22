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
