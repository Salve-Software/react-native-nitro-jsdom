#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Implements "named access on the Window object"
// (https://html.spec.whatwg.org/#named-access-on-the-window-object): an
// element with an `id`, or a `name` attribute on one of `embed`/`form`/
// `img`/`object`/`iframe`/`frame`, becomes reachable as a bare global while
// connected to the primary document — e.g. `<div id="countdown">` lets a
// script write `countdown.textContent = ...` without `getElementById`. This
// is a very common pattern in real-world CMS-embedded widget scripts (this
// project's core use case), which predate/skip `querySelector`.
//
// Pure JS, built entirely on top of already-exposed primitives (appendChild,
// removeChild, remove, innerHTML, insertAdjacentHTML, setAttribute,
// removeAttribute, the `id` accessor, querySelectorAll) via monkey-patching —
// no new native bindings, no engine-level Proxy on the global object
// (QuickJS's actual global object, the one unqualified identifiers resolve
// against, can't be swapped for a Proxy — so exposure is push-based: a real
// data property is set/deleted on `globalThis` as elements
// connect/disconnect/rename).
//
// removeChild()/remove() unregister BEFORE calling through to the native
// implementation, not after: `js_el_removeChild` invalidates the removed
// node's wrapper-cache entry (nulls its opaque pointer, since the detached
// node's underlying memory can be reused), so `getAttribute('id')` on it
// afterward silently returns nothing. innerHTML's old-content walk has the
// same requirement and already ran before-replacement for the same reason.
//
// Scope/limitations (documented rather than silently wrong):
// - Connectivity is tracked via appendChild/removeChild/remove()/innerHTML
//   assignment/insertAdjacentHTML/setAttribute("id"|"name")/removeAttribute,
//   plus a one-time walk of the initially-parsed document at install time.
//   `element.id = ...` is also hooked separately from setAttribute: it's a
//   native accessor property (ElementBindings' js_el_get_id/js_el_set_id)
//   that writes the attribute directly in C++, bypassing the JS-level
//   setAttribute monkey-patch entirely — `name` has no equivalent accessor
//   (only reachable via get/setAttribute), so it needed no separate hook.
//   Other insertion paths (insertBefore, before/after/replaceWith/append/
//   prepend, insertAdjacentElement) are not hooked — the same scope
//   CustomElementsBindings' connectivity tracking already accepted, for the
//   same reason (JS-level monkey-patching can't reach every native
//   mutation entry point without becoming a second full copy of
//   ElementBindings' insertion logic).
// - Shadow DOM content is correctly never registered: connectivity is
//   derived by walking `parentNode` up to `document.documentElement`, which
//   a shadow tree's root never reaches (per spec, named window access does
//   not cross shadow boundaries).
// - Duplicate ids/names resolve to a plain (non-live) array of the matching
//   elements, not a live `HTMLCollection` — the same static-array trade-off
//   already made for `form.elements`/`element.labels`.
// - A key already present on `globalThis` before this module claims it
//   (any real builtin/global) is never overwritten — named access only
//   fills in previously-unclaimed identifiers, and only this module's own
//   previously-claimed keys are ever deleted/reassigned later.
//
// Must run after ElementBindings, DocumentBindings, and ShadowRootBindings
// (wraps their prototypes) — CustomElementsBindings is not a hard
// dependency, but installing after it means this module's hooks wrap
// CustomElements' own wrapped appendChild/removeChild/innerHTML/
// insertAdjacentHTML/setAttribute/removeAttribute rather than the other way
// around, which is arbitrary but keeps install order consistent.
struct WindowNamedPropertiesBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
