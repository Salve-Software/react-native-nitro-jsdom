#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers <slot> assignment: Element.prototype.assignedNodes/
// assignedElements (tag-checked for "slot"), and a best-effort `slotchange`
// event. Pure JS, built entirely on top of already-exposed primitives
// (querySelectorAll, childNodes/parentNode, Element.prototype.shadowRoot,
// setAttribute/removeAttribute, ShadowRoot.prototype.innerHTML) via
// monkey-patching — Lexbor itself has no slot concept at all.
//
// Assignment algorithm: a light-DOM child of a shadow host is assigned to
// the <slot> (inside that host's shadow tree) whose `name` matches the
// child's `slot` attribute (or the unnamed/default slot, if the child has
// none), computed on demand from the live tree — never cached/stale.
//
// Scope/limitations (documented rather than silently wrong):
// - Does not implement the "first slot with a given name wins" tie-break for
//   shadow trees with duplicate slot names — assumes one slot per name.
// - assignedNodes({flatten}) falls back to the slot's own light-DOM children
//   (its fallback content) when it has no real assignment; it does not
//   recursively flatten nested slots-inside-slots.
// - slotchange fires (best-effort, comparing each slot's previous vs.
//   recomputed assignment) on: host.appendChild()/removeChild(), a
//   light-DOM child's `slot` attribute changing via setAttribute/
//   removeAttribute, and ShadowRoot.innerHTML assignment (covers newly
//   added <slot> elements getting their first, non-empty assignment — the
//   common attachShadow-then-populate pattern). NOT covered: before/after/
//   replaceWith/append/prepend on the host, or mutating the shadow tree
//   itself via appendChild/removeChild on the ShadowRoot directly.
// - Only reaches shadow roots visible via the public (mode-agnostic-for-us)
//   Element.prototype.shadowRoot getter, so a *closed*-mode shadow host
//   won't trigger slotchange from its light-DOM child mutations (the getter
//   returns null for closed mode by spec) — assignedNodes()/assignedElements()
//   on the <slot> itself are unaffected and still compute correctly.
//
// Must run after ElementBindings, EventBindings, and ShadowRootBindings.
struct SlotBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
