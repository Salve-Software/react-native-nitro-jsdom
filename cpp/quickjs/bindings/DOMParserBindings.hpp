#pragma once

// Registers globalThis.DOMParser (`.parseFromString()`) and
// document.implementation.createHTMLDocument(), each of which produces a
// genuinely separate, inert Document backed by its own LexborDocument
// instance (see doc_for_node()/register_document() in DOMBindingsInternal —
// that's what makes mutating a second document safe rather than corrupting
// the primary document's memory arena).
//
// Scope, matching real-world DOMParser usage (parsing a fetched/embedded
// HTML string and reading it back, occasionally assembling a few new nodes):
//   - Full read/query support: querySelector(All), getElementById,
//     getElementsBy{ClassName,TagName} (as static snapshots, not live
//     HTMLCollections — see below), all generic Node traversal/attribute
//     methods (those are shared with the primary document's Element/Node
//     prototypes and don't need document-specific wiring).
//   - createElement/createTextNode/createComment/createDocumentFragment,
//     appendChild/removeChild/insertBefore/replaceChild, and the
//     textContent/innerHTML setters/insertAdjacentHTML/matches()/closest()
//     inherited from Element.prototype — all resolve the correct owning
//     document dynamically, so building up a secondary document is fully
//     safe, not just read-only.
//   - title get/set (mirrors document.title's head/<title> shim).
// Not supported on secondary documents (their bindings are hardwired to the
// primary document — see ShadowRootBindings/CustomElementsBindings/
// TemplateBindings/LiveCollectionBindings): Shadow DOM (attachShadow),
// Custom Elements, <template>.content, MutationObserver, live
// HTMLCollections (.forms/.images/.scripts/.links, and
// getElementsBy{ClassName,TagName} return static arrays here instead).
// Scripts inside parsed HTML never execute (per spec, these documents are
// inert) — no script pipeline is wired to them.
//
// Must run after ElementBindings (shares its Element/Node classes/protos)
// and DocumentBindings (adds .implementation onto globalThis.document).

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class DOMParserBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
