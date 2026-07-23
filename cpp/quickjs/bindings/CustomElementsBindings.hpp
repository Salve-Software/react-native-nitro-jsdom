#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.customElements (CustomElementRegistry: define/get/
// whenDefined/upgrade) plus upgrade hooks. Pure JS, built entirely on top of
// already-exposed primitives (querySelectorAll, createElement, innerHTML,
// appendChild, removeChild, setAttribute/removeAttribute, attachShadow) via
// monkey-patching — no new native bindings.
//
// Scope/limitations (documented rather than silently wrong):
// - Elements are "upgraded" by reassigning their prototype to the registered
//   class's prototype (Object.setPrototypeOf) rather than by invoking the
//   class's constructor with the element as `this`. Real custom element
//   upgrade requires cooperation from the JS engine's class-construction
//   protocol (a real `new.target`-aware HTMLElement constructor) that this
//   sandbox doesn't implement, so constructor logic never runs — only
//   connectedCallback/disconnectedCallback/attributeChangedCallback do.
// - Upgrade triggers: document.createElement(), a customElements.define()
//   call rescanning the current document (covers elements already present
//   from the initial HTML), Element/ShadowRoot.innerHTML assignment, and
//   insertAdjacentHTML. Other insertion paths (before/after/replaceWith/
//   append/prepend) are not hooked.
// - The define()-time rescan walks document.querySelectorAll(name) only, so
//   it does not cross shadow boundaries: elements already sitting inside an
//   existing shadow root are not retroactively upgraded when a matching tag
//   is define()'d afterward. (Populating that shadow root's innerHTML after
//   the define() call still upgrades them, per the innerHTML trigger above.)
//   Calling customElements.upgrade(root) manually on that shadow root (or any
//   ancestor whose shadow-including subtree contains it) covers this case.
// - customElements.upgrade(root) walks root's shadow-including INCLUSIVE
//   descendants (root itself, if an element, plus everything reachable via
//   the same light-DOM + shadow traversal the other hooks use). Throws
//   TypeError if root isn't a Node (matches the spec: null/undefined/plain
//   objects are rejected, not silently no-op'd).
// - attachShadow() is wrapped purely to tag the host element with an
//   internal, mode-agnostic reference to its shadow root (bypassing the
//   closed-mode privacy Element.prototype.shadowRoot enforces for user
//   script), so appendChild/removeChild/innerHTML connectivity bookkeeping
//   can descend into shadow trees the same way it walks light-DOM children.
//
// Must run after ElementBindings, DocumentBindings, and ShadowRootBindings
// (wraps their innerHTML/createElement/appendChild/removeChild/attachShadow).
struct CustomElementsBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
