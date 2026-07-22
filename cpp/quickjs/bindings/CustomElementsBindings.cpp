#include "CustomElementsBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kCustomElementsBootstrapScript = R"JS(
(function() {
  var RESERVED_NAMES = [
    'annotation-xml', 'color-profile', 'font-face', 'font-face-src',
    'font-face-uri', 'font-face-format', 'font-face-name', 'missing-glyph',
  ];
  var NAME_RE = /^[a-z][a-z0-9._-]*-[a-z0-9._-]*$/;

  function isValidName(name) {
    if (typeof name !== 'string' || name === '') return false;
    if (RESERVED_NAMES.indexOf(name) !== -1) return false;
    return NAME_RE.test(name);
  }

  // Elements/ShadowRoots don't naturally track their own document, so
  // connectivity is derived by walking parentNode (crossing shadow
  // boundaries via .host) until documentElement is reached.
  function isConnected(node) {
    var docEl = document.documentElement;
    var n = node;
    while (n) {
      if (n === docEl) return true;
      n = ('host' in n) ? n.host : n.parentNode;
    }
    return false;
  }

  // All elements ever upgraded, so structural mutations (appendChild,
  // removeChild, innerHTML, insertAdjacentHTML) can re-check connectivity
  // without needing precise subtree-diffing at each call site.
  var upgradedElements = [];

  function upgradeElement(el) {
    if (!el || el.__ceUpgraded) return;
    var tag = el.tagName ? el.tagName.toLowerCase() : null;
    if (!tag) return;
    var def = customElements._defs[tag];
    if (!def) return;

    Object.setPrototypeOf(el, def.ctor.prototype);
    Object.defineProperty(el, '__ceUpgraded', { value: true, configurable: true });
    Object.defineProperty(el, '__ceConnected', { value: false, writable: true, configurable: true });
    upgradedElements.push(el);

    def.observedAttributes.forEach(function(attr) {
      if (el.hasAttribute(attr) && typeof el.attributeChangedCallback === 'function') {
        try { el.attributeChangedCallback(attr, null, el.getAttribute(attr), null); } catch (e) {}
      }
    });

    if (isConnected(el)) {
      el.__ceConnected = true;
      if (typeof el.connectedCallback === 'function') {
        try { el.connectedCallback(); } catch (e) {}
      }
    }
  }

  function upgradeAllMatching(name) {
    var found = document.querySelectorAll(name);
    for (var i = 0; i < found.length; i++) upgradeElement(found[i]);
  }

  function upgradeSubtree(root) {
    if (!root || typeof root.querySelectorAll !== 'function') return;
    var found = root.querySelectorAll('*');
    for (var i = 0; i < found.length; i++) upgradeElement(found[i]);
  }

  // Re-checks connectivity of every upgraded element, firing
  // connectedCallback/disconnectedCallback on transitions. Called after any
  // primitive that can move nodes into/out of the connected tree.
  function syncConnected() {
    upgradedElements.forEach(function(el) {
      var connected = isConnected(el);
      if (connected === el.__ceConnected) return;
      el.__ceConnected = connected;
      if (connected) {
        if (typeof el.connectedCallback === 'function') {
          try { el.connectedCallback(); } catch (e) {}
        }
      } else {
        if (typeof el.disconnectedCallback === 'function') {
          try { el.disconnectedCallback(); } catch (e) {}
        }
      }
    });
  }

  // ── CustomElementRegistry ────────────────────────────────────────────────

  function CustomElementRegistry() {
    this._defs = Object.create(null);
    this._pending = Object.create(null);
    this._ctors = [];
  }

  CustomElementRegistry.prototype.define = function(name, ctor, options) {
    if (typeof ctor !== 'function') {
      throw new TypeError("Failed to execute 'define' on 'CustomElementRegistry': " +
          "the constructor must be a function");
    }
    if (!isValidName(name)) {
      throw new DOMException("'" + name + "' is not a valid custom element name", 'SyntaxError');
    }
    if (this._defs[name]) {
      throw new DOMException(
          "the name \"" + name + "\" has already been used with this registry", 'NotSupportedError');
    }
    if (this._ctors.indexOf(ctor) !== -1) {
      throw new DOMException(
          'this constructor has already been used with this registry', 'NotSupportedError');
    }

    var observed = ctor.observedAttributes;
    this._defs[name] = {
      ctor: ctor,
      observedAttributes: Array.isArray(observed) ? observed : [],
    };
    this._ctors.push(ctor);

    // Covers elements already present in the document (e.g. from the
    // initial HTML parse, or created before this define() call).
    upgradeAllMatching(name);
    syncConnected();

    if (this._pending[name]) {
      this._pending[name].resolve();
      delete this._pending[name];
    }
  };

  CustomElementRegistry.prototype.get = function(name) {
    var def = this._defs[name];
    return def ? def.ctor : undefined;
  };

  CustomElementRegistry.prototype.whenDefined = function(name) {
    if (!isValidName(name)) {
      return Promise.reject(new DOMException("'" + name + "' is not a valid custom element name", 'SyntaxError'));
    }
    if (this._defs[name]) return Promise.resolve();
    if (!this._pending[name]) {
      var resolveFn;
      var promise = new Promise(function(resolve) { resolveFn = resolve; });
      this._pending[name] = { promise: promise, resolve: resolveFn };
    }
    return this._pending[name].promise;
  };

  globalThis.CustomElementRegistry = CustomElementRegistry;
  globalThis.customElements = new CustomElementRegistry();

  // ── Upgrade hooks: document.createElement ────────────────────────────────

  var origCreateElement = document.createElement;
  document.createElement = function(tag) {
    var el = origCreateElement.call(document, tag);
    upgradeElement(el);
    return el;
  };

  // ── Upgrade hooks: Element.prototype.innerHTML / ShadowRoot.prototype.innerHTML ──

  function patchInnerHTML(proto) {
    var desc = Object.getOwnPropertyDescriptor(proto, 'innerHTML');
    if (!desc || !desc.set) return;
    var origSet = desc.set;
    Object.defineProperty(proto, 'innerHTML', {
      get: desc.get,
      set: function(html) {
        origSet.call(this, html);
        upgradeSubtree(this);
        syncConnected();
      },
      enumerable: desc.enumerable,
      configurable: true,
    });
  }
  patchInnerHTML(Element.prototype);
  patchInnerHTML(ShadowRoot.prototype);

  // ── Upgrade hooks: Element.prototype.insertAdjacentHTML ──────────────────

  var origInsertAdjacentHTML = Element.prototype.insertAdjacentHTML;
  Element.prototype.insertAdjacentHTML = function(position, html) {
    origInsertAdjacentHTML.call(this, position, html);
    upgradeSubtree(this);
    if (this.parentNode) upgradeSubtree(this.parentNode);
    syncConnected();
  };

  // ── Connect/disconnect hooks: Node.prototype.appendChild / removeChild ───

  var origAppendChild = Node.prototype.appendChild;
  Node.prototype.appendChild = function(child) {
    var result = origAppendChild.call(this, child);
    syncConnected();
    return result;
  };

  var origRemoveChild = Node.prototype.removeChild;
  Node.prototype.removeChild = function(child) {
    var result = origRemoveChild.call(this, child);
    syncConnected();
    return result;
  };

  // ── attributeChangedCallback hooks: setAttribute / removeAttribute ───────

  function observedDefFor(el) {
    if (!el.__ceUpgraded) return null;
    var tag = el.tagName ? el.tagName.toLowerCase() : null;
    return tag ? customElements._defs[tag] : null;
  }

  var origSetAttribute = Element.prototype.setAttribute;
  Element.prototype.setAttribute = function(name, value) {
    var def = observedDefFor(this);
    var oldValue = def ? this.getAttribute(name) : null;
    origSetAttribute.call(this, name, value);
    if (def && def.observedAttributes.indexOf(name) !== -1 &&
        typeof this.attributeChangedCallback === 'function') {
      try { this.attributeChangedCallback(name, oldValue, this.getAttribute(name), null); } catch (e) {}
    }
  };

  var origRemoveAttribute = Element.prototype.removeAttribute;
  Element.prototype.removeAttribute = function(name) {
    var def = observedDefFor(this);
    var oldValue = def ? this.getAttribute(name) : null;
    origRemoveAttribute.call(this, name);
    if (def && def.observedAttributes.indexOf(name) !== -1 &&
        typeof this.attributeChangedCallback === 'function') {
      try { this.attributeChangedCallback(name, oldValue, null, null); } catch (e) {}
    }
  };
})();
)JS";

} // namespace

void CustomElementsBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kCustomElementsBootstrapScript, strlen(kCustomElementsBootstrapScript),
                            "<custom-elements-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
