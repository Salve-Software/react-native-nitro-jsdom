#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.customElements (CustomElementRegistry: define/get/
// whenDefined) plus upgrade hooks. Pure JS, built entirely on top of
// already-exposed primitives (querySelectorAll, createElement, innerHTML,
// appendChild, removeChild, setAttribute/removeAttribute) via monkey-patching
// — no new native bindings.
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
//
// Must run after ElementBindings, DocumentBindings, and ShadowRootBindings
// (wraps their innerHTML/createElement/appendChild/removeChild).
struct CustomElementsBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
