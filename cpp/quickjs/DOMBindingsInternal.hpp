#pragma once

// Internal header shared across the QuickJS binding translation units
// (ElementBindings.cpp, DocumentBindings.cpp, EventBindings.cpp, TimerBindings.cpp,
// WindowBindings.cpp, FetchBindings.cpp, MutationObservers.cpp, ...).
//
// DOMBindings::install() (DOMBindings.cpp) is the only orchestrator: it calls each
// of the *Bindings::install(ctx) entry points in order. Everything below is the
// small set of helpers those entry points need from one another.

#include "quickjs.h"
#include <string>
#include <vector>

// Forward-declared at global scope (not inside our namespace) so this matches
// the real lxb_dom_element_t/lxb_dom_node_t typedefs from lexbor's own headers
// bit-for-bit when both are included in the same translation unit.
struct lxb_dom_element;
struct lxb_dom_node;
typedef struct lxb_dom_element lxb_dom_element_t;
typedef struct lxb_dom_node lxb_dom_node_t;

namespace margelo::nitro::nitrojsdom {

struct RuntimeContext;
class LexborDocument;

// ── Element / Node class IDs + wrapping ──────────────────────────────────────
// js_element_class_id wraps lxb_dom_element_t* — real element nodes only.
// js_node_class_id    wraps lxb_dom_node_t*    — text, comment, document, etc.
//
// make_element() dispatches on the node type: Element nodes get the element
// class (which exposes getAttribute, classList, etc.); all other nodes get the
// node class (Node traversal only). This prevents non-element nodes from
// reaching lxb_dom_element_* calls.
//
// js_shadow_root_class_id wraps lxb_dom_shadow_root_t* (see ShadowRootBindings).
//
// unwrap_element() returns non-null only for element-class objects.
// unwrap_node()    returns non-null for any of the three (shared Node methods
//                  like appendChild/childNodes work the same way on a
//                  ShadowRoot, since it embeds a document fragment's layout).

extern JSClassID js_element_class_id;
extern JSClassID js_node_class_id;
extern JSClassID js_shadow_root_class_id;

JSValue make_element(JSContext* ctx, void* node_or_el);
JSValue make_element_array(JSContext* ctx, const std::vector<void*>& elements);
lxb_dom_element_t* unwrap_element(JSContext* ctx, JSValue val);
lxb_dom_node_t*    unwrap_node(JSContext* ctx, JSValue val);
std::string serialize_node(lxb_dom_node_t* node);

// ── RuntimeContext access ─────────────────────────────────────────────────────

RuntimeContext* get_ctx(JSContext* ctx);
LexborDocument* get_doc(JSContext* ctx);

// ── Node wrapper cache ────────────────────────────────────────────────────────

void invalidate_node_cache(JSContext* ctx, RuntimeContext* rctx, void* node);
void invalidate_node_cache_batch(JSContext* ctx, RuntimeContext* rctx, const std::vector<void*>& nodes);
void clear_node_cache(JSContext* ctx, RuntimeContext* rctx);

// Same as invalidate_node_cache_batch, but also invalidates every descendant
// of each given node. Use before destroying a subtree with
// lxb_dom_node_destroy_deep(): that frees descendants too, and a JS wrapper
// left pointing at a freed descendant is a use-after-free waiting to happen.
void invalidate_node_cache_deep_batch(JSContext* ctx, RuntimeContext* rctx, const std::vector<void*>& roots);

// ── Accessor property helper ──────────────────────────────────────────────────

using GetterFn = JSValue (*)(JSContext*, JSValue);
using SetterFn = JSValue (*)(JSContext*, JSValue, JSValue);

void define_prop(JSContext* ctx, JSValue obj, const char* name, GetterFn getter, SetterFn setter = nullptr);

// Sets the WHATWG DOM Node.ELEMENT_NODE-style numeric constants (ELEMENT_NODE,
// TEXT_NODE, COMMENT_NODE, ...) as read-only enumerable properties on `obj`.
// Values match lxb_dom_node_type_t, which already mirrors the DOM spec numbering.
void define_node_type_constants(JSContext* ctx, JSValue obj);

// ── Global constructor helper (instanceof support) ────────────────────────────

JSValue js_illegal_constructor(JSContext* ctx, JSValue this_val, int argc, JSValue* argv);
JSValue define_global_constructor(JSContext* ctx, const char* name, JSValue proto);

// ── Misc shared helpers ───────────────────────────────────────────────────────

// Reads a boolean-ish property off any JS object (used by Event dispatch to read
// bubbles/cancelable/defaultPrevented/internal propagation flags).
bool get_bool_prop(JSContext* ctx, JSValue obj, const char* name);

// Builds a CSS selector matching elements with every class in `names` (a
// space-separated list), e.g. "foo bar" -> ".foo.bar". Shared by
// Element::getElementsByClassName and document.getElementsByClassName.
std::string classNames_to_selector(const std::string& names);

// camelCase <-> kebab-case conversion shared by DatasetBindings (data-foo-bar
// <-> fooBar) and StyleBindings (background-color <-> backgroundColor).
std::string attr_suffix_to_camel(const std::string& suffix);
std::string camel_to_attr_suffix(const std::string& camel);

} // namespace margelo::nitro::nitrojsdom
