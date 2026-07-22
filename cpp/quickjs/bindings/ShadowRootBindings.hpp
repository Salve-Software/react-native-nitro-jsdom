#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers the ShadowRoot class (host/mode/innerHTML/querySelector(All),
// plus Node-proto methods like appendChild/childNodes via prototype chain)
// and Element.prototype.attachShadow()/shadowRoot.
//
// A ShadowRoot wraps a real lxb_dom_shadow_root_t*, which embeds a
// lxb_dom_document_fragment_t as its first member — so it shares the
// document fragment's node-tree layout, and generic Node methods work on it
// unmodified. Lexbor has no attachShadow-equivalent linking a host element
// to its shadow root, so that association is tracked in
// RuntimeContext::element_shadow_roots.
//
// No slotting/encapsulation/event retargeting — this models just enough of
// Shadow DOM for scripts that create/query/populate a scoped subtree.
//
// Must run after ElementBindings (globalThis.Element, js_node_class_id's
// proto) and DOMExceptionBindings (NotSupportedError on double-attach).
struct ShadowRootBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
