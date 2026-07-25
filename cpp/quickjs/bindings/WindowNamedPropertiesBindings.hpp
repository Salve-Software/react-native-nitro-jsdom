#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Implements "named access on the Window object"
// (https://html.spec.whatwg.org/#named-access-on-the-window-object): an
// element with an `id`, or a `name` attribute on `embed`/`form`/`img`/
// `object`/`iframe`/`frame`, becomes reachable as a bare global while
// connected to the primary document.
//
// Pure JS, monkey-patching appendChild/removeChild/remove/innerHTML/
// textContent/insertAdjacentHTML/setAttribute/removeAttribute/the `id`
// accessor — no new native bindings, no engine-level Proxy (QuickJS's real
// global object can't be swapped for one), so exposure is push-based.
//
// Duplicate ids/names resolve to a plain array, not a live `HTMLCollection`
// (same trade-off as `form.elements`/`element.labels`). insertBefore/before/
// after/replaceWith/append/prepend/insertAdjacentElement are not hooked
// (same scope CustomElementsBindings' connectivity tracking accepted).
// Never overwrites a key already present on `globalThis`.
//
// Must run last in DOMBindings::install (after localStorage/sessionStorage
// exist, so it never claims those keys).
struct WindowNamedPropertiesBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
