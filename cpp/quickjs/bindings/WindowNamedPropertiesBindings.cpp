#include "WindowNamedPropertiesBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kWindowNamedPropertiesBootstrapScript = R"JS(
(function() {
  var NAME_ATTR_TAGS = { embed: 1, form: 1, img: 1, object: 1, iframe: 1, frame: 1 };

  // registry: key -> element[] currently claiming it. owned: key -> true once
  // this module has written globalThis[key] (so it knows it's safe to delete
  // or reassign later without clobbering something else's global).
  var registry = Object.create(null);
  var owned = Object.create(null);

  function isConnected(el) {
    var docEl = document.documentElement;
    var n = el;
    while (n) {
      if (n === docEl) return true;
      n = n.parentNode;
    }
    return false;
  }

  function refreshKey(key) {
    var els = registry[key];
    if (!els || els.length === 0) {
      if (owned[key]) { delete globalThis[key]; delete owned[key]; }
      return;
    }
    if (!owned[key] && (key in globalThis)) return; // don't clobber a real global
    globalThis[key] = els.length === 1 ? els[0] : els.slice();
    owned[key] = true;
  }

  function addToRegistry(key, el) {
    if (!key) return;
    var els = registry[key] || (registry[key] = []);
    if (els.indexOf(el) === -1) els.push(el);
    refreshKey(key);
  }

  function removeFromRegistry(key, el) {
    if (!key) return;
    var els = registry[key];
    if (!els) return;
    var idx = els.indexOf(el);
    if (idx !== -1) els.splice(idx, 1);
    refreshKey(key);
  }

  function keysFor(el) {
    var keys = [];
    var id = el.getAttribute ? el.getAttribute('id') : null;
    if (id) keys.push(id);
    var tag = el.tagName ? el.tagName.toLowerCase() : '';
    if (NAME_ATTR_TAGS[tag]) {
      var name = el.getAttribute('name');
      if (name) keys.push(name);
    }
    return keys;
  }

  function registerIfConnected(el) {
    if (!el || el.nodeType !== 1 || !isConnected(el)) return;
    keysFor(el).forEach(function(k) { addToRegistry(k, el); });
  }

  function unregister(el) {
    if (!el || el.nodeType !== 1) return;
    keysFor(el).forEach(function(k) { removeFromRegistry(k, el); });
  }

  function registerSubtree(root) {
    if (!root) return;
    if (root.nodeType === 1) registerIfConnected(root);
    if (typeof root.querySelectorAll !== 'function') return;
    var found = root.querySelectorAll('*');
    for (var i = 0; i < found.length; i++) registerIfConnected(found[i]);
  }

  function unregisterSubtree(root) {
    if (!root) return;
    if (root.nodeType === 1) unregister(root);
    if (typeof root.querySelectorAll !== 'function') return;
    var found = root.querySelectorAll('*');
    for (var i = 0; i < found.length; i++) unregister(found[i]);
  }

  // ── Initial population: elements already present in the parsed document ──
  if (document.documentElement) registerSubtree(document.documentElement);

  // ── appendChild / removeChild ─────────────────────────────────────────
  var DOCUMENT_FRAGMENT_NODE = 11;

  var origAppendChild = Node.prototype.appendChild;
  Node.prototype.appendChild = function(child) {
    var moved = (child && child.nodeType === DOCUMENT_FRAGMENT_NODE)
        ? Array.prototype.slice.call(child.childNodes)
        : [child];
    var result = origAppendChild.call(this, child);
    moved.forEach(registerSubtree);
    return result;
  };

  var origRemoveChild = Node.prototype.removeChild;
  Node.prototype.removeChild = function(child) {
    unregisterSubtree(child);
    return origRemoveChild.call(this, child);
  };

  var origRemove = Element.prototype.remove;
  Element.prototype.remove = function() {
    unregisterSubtree(this);
    return origRemove.call(this);
  };

  // ── innerHTML ────────────────────────────────────────────────────────
  var innerHTMLDesc = Object.getOwnPropertyDescriptor(Element.prototype, 'innerHTML');
  if (innerHTMLDesc && innerHTMLDesc.set) {
    var origInnerHTMLSet = innerHTMLDesc.set;
    Object.defineProperty(Element.prototype, 'innerHTML', {
      get: innerHTMLDesc.get,
      set: function(html) {
        unregisterSubtree(this); // walk the OLD content before it's replaced
        origInnerHTMLSet.call(this, html);
        registerSubtree(this); // register the NEW content
      },
      enumerable: innerHTMLDesc.enumerable,
      configurable: true,
    });
  }

  // ── insertAdjacentHTML ──────────────────────────────────────────────
  var origInsertAdjacentHTML = Element.prototype.insertAdjacentHTML;
  Element.prototype.insertAdjacentHTML = function(position, html) {
    origInsertAdjacentHTML.call(this, position, html);
    var pos = String(position).toLowerCase();
    registerSubtree((pos === 'beforebegin' || pos === 'afterend') ? (this.parentNode || this) : this);
  };

  // ── element.id (native accessor property — bypasses setAttribute) ──
  var idDesc = Object.getOwnPropertyDescriptor(Element.prototype, 'id');
  if (idDesc && idDesc.set) {
    var origIdSet = idDesc.set;
    Object.defineProperty(Element.prototype, 'id', {
      get: idDesc.get,
      set: function(value) {
        var oldValue = this.getAttribute('id');
        origIdSet.call(this, value);
        if (oldValue) removeFromRegistry(oldValue, this);
        registerIfConnected(this);
      },
      enumerable: idDesc.enumerable,
      configurable: true,
    });
  }

  // ── setAttribute / removeAttribute (id/name changes) ───────────────
  function isTrackedAttr(el, name) {
    var lower = String(name).toLowerCase();
    if (lower === 'id') return true;
    var tag = el.tagName ? el.tagName.toLowerCase() : '';
    return lower === 'name' && !!NAME_ATTR_TAGS[tag];
  }

  var origSetAttribute = Element.prototype.setAttribute;
  Element.prototype.setAttribute = function(name, value) {
    var tracked = isTrackedAttr(this, name);
    var oldValue = tracked ? this.getAttribute(name) : null;
    origSetAttribute.call(this, name, value);
    if (tracked) {
      if (oldValue) removeFromRegistry(oldValue, this);
      registerIfConnected(this);
    }
  };

  var origRemoveAttribute = Element.prototype.removeAttribute;
  Element.prototype.removeAttribute = function(name) {
    var tracked = isTrackedAttr(this, name);
    var oldValue = tracked ? this.getAttribute(name) : null;
    origRemoveAttribute.call(this, name);
    if (tracked && oldValue) removeFromRegistry(oldValue, this);
  };
})();
)JS";

} // namespace

void WindowNamedPropertiesBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kWindowNamedPropertiesBootstrapScript, strlen(kWindowNamedPropertiesBootstrapScript),
                            "<window-named-properties-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
