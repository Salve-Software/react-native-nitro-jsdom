#include "SlotBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kSlotBootstrapScript = R"JS(
(function() {
  function isSlotElement(el) {
    return !!el && el.tagName === 'SLOT';
  }

  // Elements can carry a slot="..." attribute; anything else (text nodes,
  // elements without the attribute) is assigned to the default slot ("").
  function effectiveSlotName(node) {
    if (node && node.nodeType === 1 && typeof node.getAttribute === 'function') {
      return node.getAttribute('slot') || '';
    }
    return '';
  }

  // Walks up from a node living INSIDE a shadow tree (e.g. a <slot>) to find
  // its containing ShadowRoot.
  function containingShadowRoot(node) {
    var n = node ? node.parentNode : null;
    while (n) {
      if (n instanceof ShadowRoot) return n;
      n = n.parentNode;
    }
    return null;
  }

  // For a light-DOM child of some shadow host, returns that host's shadow
  // root (or null if its parent has none / it's closed-mode).
  function hostShadowRootFor(node) {
    var parent = node ? node.parentNode : null;
    return (parent && parent.shadowRoot) ? parent.shadowRoot : null;
  }

  function slotName(slotEl) {
    return slotEl.getAttribute('name') || '';
  }

  // Nodes assigned to `slotEl`: light-DOM children of the shadow root's host
  // whose effective slot name matches this slot's name, in host child order.
  function computeAssigned(slotEl) {
    var root = containingShadowRoot(slotEl);
    if (!root) return [];
    var host = root.host;
    if (!host) return [];
    var name = slotName(slotEl);
    var result = [];
    var child = host.firstChild;
    while (child) {
      if (effectiveSlotName(child) === name) result.push(child);
      child = child.nextSibling;
    }
    return result;
  }

  Element.prototype.assignedNodes = function(options) {
    if (!isSlotElement(this)) return [];
    var assigned = computeAssigned(this);
    if (assigned.length === 0 && options && options.flatten) {
      var fallback = [];
      var c = this.firstChild;
      while (c) { fallback.push(c); c = c.nextSibling; }
      return fallback;
    }
    return assigned;
  };

  Element.prototype.assignedElements = function(options) {
    return this.assignedNodes(options).filter(function(n) { return n.nodeType === 1; });
  };

  // ── slotchange (best-effort — see SlotBindings.hpp for exact triggers) ────

  function sameAssignment(a, b) {
    if (a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function notifySlotChange(root) {
    if (!root || typeof root.querySelectorAll !== 'function') return;
    var slots = root.querySelectorAll('slot');
    for (var i = 0; i < slots.length; i++) {
      var slot = slots[i];
      var next = computeAssigned(slot);
      var prev = slot.__slotAssigned || [];
      if (!sameAssignment(prev, next)) {
        slot.__slotAssigned = next;
        slot.dispatchEvent(new Event('slotchange', { bubbles: true }));
      }
    }
  }

  var origAppendChild = Node.prototype.appendChild;
  Node.prototype.appendChild = function(child) {
    var result = origAppendChild.call(this, child);
    if (this.shadowRoot) notifySlotChange(this.shadowRoot);
    return result;
  };

  var origRemoveChild = Node.prototype.removeChild;
  Node.prototype.removeChild = function(child) {
    var result = origRemoveChild.call(this, child);
    if (this.shadowRoot) notifySlotChange(this.shadowRoot);
    return result;
  };

  var origSetAttribute = Element.prototype.setAttribute;
  Element.prototype.setAttribute = function(name, value) {
    var result = origSetAttribute.call(this, name, value);
    if (String(name).toLowerCase() === 'slot') {
      var root = hostShadowRootFor(this);
      if (root) notifySlotChange(root);
    }
    return result;
  };

  var origRemoveAttribute = Element.prototype.removeAttribute;
  Element.prototype.removeAttribute = function(name) {
    var result = origRemoveAttribute.call(this, name);
    if (String(name).toLowerCase() === 'slot') {
      var root = hostShadowRootFor(this);
      if (root) notifySlotChange(root);
    }
    return result;
  };

  var shadowInnerHTMLDesc = Object.getOwnPropertyDescriptor(ShadowRoot.prototype, 'innerHTML');
  if (shadowInnerHTMLDesc && shadowInnerHTMLDesc.set) {
    var origShadowInnerHTMLSet = shadowInnerHTMLDesc.set;
    Object.defineProperty(ShadowRoot.prototype, 'innerHTML', {
      get: shadowInnerHTMLDesc.get,
      set: function(html) {
        origShadowInnerHTMLSet.call(this, html);
        notifySlotChange(this);
      },
      enumerable: shadowInnerHTMLDesc.enumerable,
      configurable: true,
    });
  }
})();
)JS";

} // namespace

void SlotBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kSlotBootstrapScript, strlen(kSlotBootstrapScript),
                            "<slot-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
