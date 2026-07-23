#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.XMLSerializer with serializeToString(node), backed by
// the same serialize_node() helper Element.prototype.outerHTML already uses
// (DOMBindingsInternal.hpp) — serializes the given node and its subtree.
//
// Scope: only serializeToString(node) is implemented (DOMParser.parseFromString
// is out of scope — see docs/overview.md's v0.10 notes, it would need a real
// second Document). `node` must be a wrapped Element/Node/ShadowRoot (i.e. not
// `document` itself, which is a plain JS object with no native node behind
// it) — pass document.documentElement instead, same as any other subtree
// serialization in this sandbox.
//
// Must run after ElementBindings (unwrap_node/serialize_node need
// js_element_class_id/js_node_class_id to exist).
struct XmlSerializerBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
