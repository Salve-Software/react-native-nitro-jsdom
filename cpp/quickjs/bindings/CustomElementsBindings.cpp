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

  // No global registry of upgraded elements: every hook below scopes itself
  // to the subtree it actually touched (via a local, naturally
  // garbage-collectible JS array), so nothing here can leak memory or grow
  // unbounded, and no hook costs more than the size of the DOM it mutated.

  function fireConnectivityChange(el, connected) {
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
  }

  function upgradeElement(el) {
    if (!el || el.__ceUpgraded) return;
    var tag = el.tagName ? el.tagName.toLowerCase() : null;
    if (!tag) return;
    var def = customElements._defs[tag];
    if (!def) return;

    Object.setPrototypeOf(el, def.ctor.prototype);
    Object.defineProperty(el, '__ceUpgraded', { value: true, configurable: true });
    Object.defineProperty(el, '__ceConnected', { value: false, writable: true, configurable: true });

    def.observedAttributes.forEach(function(attr) {
      if (el.hasAttribute(attr) && typeof el.attributeChangedCallback === 'function') {
        try { el.attributeChangedCallback(attr, null, el.getAttribute(attr), null); } catch (e) {}
      }
    });

    if (isConnected(el)) fireConnectivityChange(el, true);
  }

  function upgradeAllMatching(name) {
    var found = document.querySelectorAll(name);
    for (var i = 0; i < found.length; i++) upgradeElement(found[i]);
  }

  // Returns the shadow root attached to `el`, if any, regardless of mode.
  // Element.prototype.shadowRoot hides closed-mode roots from user script by
  // spec, but our own upgrade/connectivity bookkeeping isn't user script —
  // it needs to see into every shadow tree the same way the UA internally
  // does. Populated by the attachShadow() patch further down.
  function shadowRootOf(el) {
    return el ? el.__ceShadowRoot : null;
  }

  // Collects every descendant of `root` (light DOM), recursing into root's
  // OWN attached shadow root (a custom element's shadow content isn't a
  // light-DOM child of anything — querySelectorAll('*') alone would never
  // find it) as well as each descendant's shadow root, so custom elements
  // living inside shadow trees aren't invisible to the subtree-scoped hooks
  // below. Does not include `root` itself.
  function collectDescendantsIncludingShadow(root, out) {
    if (!root) return;
    var rootSr = shadowRootOf(root);
    if (rootSr) collectDescendantsIncludingShadow(rootSr, out);
    if (typeof root.querySelectorAll !== 'function') return;
    var found = root.querySelectorAll('*');
    for (var i = 0; i < found.length; i++) {
      var el = found[i];
      out.push(el);
      var sr = shadowRootOf(el);
      if (sr) collectDescendantsIncludingShadow(sr, out);
    }
  }

  function upgradeSubtree(root) {
    var found = [];
    collectDescendantsIncludingShadow(root, found);
    for (var i = 0; i < found.length; i++) upgradeElement(found[i]);
  }

  // Fires disconnectedCallback for every already-upgraded, already-connected
  // custom element currently in root's subtree (light DOM + nested shadow
  // trees). Call this on the OLD content of a node right BEFORE it's
  // wholesale-replaced (innerHTML assignment unconditionally destroys every
  // existing child), while it's still walkable.
  function disconnectSubtree(root) {
    var found = [];
    collectDescendantsIncludingShadow(root, found);
    for (var i = 0; i < found.length; i++) {
      var el = found[i];
      if (el.__ceUpgraded && el.__ceConnected) fireConnectivityChange(el, false);
    }
  }

  // Re-checks connectivity of `node`, and every descendant across light DOM
  // and nested shadow trees, firing connectedCallback/disconnectedCallback on
  // transitions. Used by appendChild/removeChild, which know exactly which
  // subtree moved, so this stays O(size of that subtree) regardless of how
  // many custom elements exist elsewhere in the document.
  function syncConnectedSubtree(node) {
    if (!node) return;
    if (node.__ceUpgraded) {
      var connected = isConnected(node);
      if (connected !== node.__ceConnected) fireConnectivityChange(node, connected);
    }
    var found = [];
    collectDescendantsIncludingShadow(node, found);
    for (var i = 0; i < found.length; i++) {
      var el = found[i];
      if (!el.__ceUpgraded) continue;
      var elConnected = isConnected(el);
      if (elConnected !== el.__ceConnected) fireConnectivityChange(el, elConnected);
    }
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

    upgradeAllMatching(name);

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

  // upgrade(root): shadow-including INCLUSIVE descendants — root itself (if
  // it's an element) plus everything upgradeSubtree already walks (which is
  // exclusive of root, since its other callers — innerHTML/insertAdjacentHTML
  // patches below — always pass a pre-existing container that was never
  // itself a fresh upgrade candidate).
  CustomElementRegistry.prototype.upgrade = function(root) {
    if (!(root instanceof Node)) {
      throw new TypeError(
          "Failed to execute 'upgrade' on 'CustomElementRegistry': parameter 1 is not of type 'Node'");
    }
    var ELEMENT_NODE = 1;
    if (root.nodeType === ELEMENT_NODE) upgradeElement(root);
    upgradeSubtree(root);
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
        disconnectSubtree(this); // walk the OLD content before it's replaced
        origSet.call(this, html);
        upgradeSubtree(this); // upgrade/connect the NEW content
      },
      enumerable: desc.enumerable,
      configurable: true,
    });
  }
  patchInnerHTML(Element.prototype);
  patchInnerHTML(ShadowRoot.prototype);

  // ── Track attached shadow roots for internal (mode-agnostic) traversal ───

  var origAttachShadow = Element.prototype.attachShadow;
  Element.prototype.attachShadow = function(options) {
    var root = origAttachShadow.call(this, options);
    Object.defineProperty(this, '__ceShadowRoot', { value: root, configurable: true });
    return root;
  };

  // ── Upgrade hooks: Element.prototype.insertAdjacentHTML ──────────────────

  // insertAdjacentHTML only ever adds content, never removes any — no
  // existing element can be disconnected by it, so there's nothing to sync
  // beyond upgrading (and, via upgradeElement, connecting) what's new.
  var origInsertAdjacentHTML = Element.prototype.insertAdjacentHTML;
  Element.prototype.insertAdjacentHTML = function(position, html) {
    origInsertAdjacentHTML.call(this, position, html);
    upgradeSubtree(this);
    var pos = String(position).toLowerCase();
    if ((pos === 'beforebegin' || pos === 'afterend') && this.parentNode) {
      upgradeSubtree(this.parentNode);
    }
  };

  // ── Connect/disconnect hooks: Node.prototype.appendChild / removeChild ───

  var DOCUMENT_FRAGMENT_NODE = 11;

  var origAppendChild = Node.prototype.appendChild;
  Node.prototype.appendChild = function(child) {
    var moved = (child && child.nodeType === DOCUMENT_FRAGMENT_NODE)
        ? Array.prototype.slice.call(child.childNodes)
        : [child];
    var result = origAppendChild.call(this, child);
    moved.forEach(syncConnectedSubtree);
    return result;
  };

  var origRemoveChild = Node.prototype.removeChild;
  Node.prototype.removeChild = function(child) {
    var result = origRemoveChild.call(this, child);
    syncConnectedSubtree(child);
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
