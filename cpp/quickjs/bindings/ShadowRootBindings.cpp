#include "ShadowRootBindings.hpp"
#include "DOMExceptionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassDef js_shadow_root_class = { "ShadowRoot", .finalizer = nullptr };

lxb_dom_shadow_root_t* unwrap_shadow_root(JSValue val) {
  return static_cast<lxb_dom_shadow_root_t*>(JS_GetOpaque(val, js_shadow_root_class_id));
}

JSValue make_shadow_root(JSContext* ctx, void* shadow_ptr) {
  if (!shadow_ptr) return JS_NULL;

  RuntimeContext* rctx = get_ctx(ctx);
  if (rctx) {
    auto it = rctx->node_wrapper_cache.find(shadow_ptr);
    if (it != rctx->node_wrapper_cache.end()) {
      return JS_DupValue(ctx, *static_cast<JSValue*>(it->second));
    }
  }

  JSValue obj = JS_NewObjectClass(ctx, js_shadow_root_class_id);
  JS_SetOpaque(obj, shadow_ptr);
  if (rctx) rctx->node_wrapper_cache[shadow_ptr] = new JSValue(JS_DupValue(ctx, obj));
  return obj;
}

JSValue js_shadow_get_host(JSContext* ctx, JSValue this_val) {
  auto* shadow = unwrap_shadow_root(this_val);
  if (!shadow) return JS_NULL;
  return make_element(ctx, shadow->host);
}

JSValue js_shadow_get_mode(JSContext* ctx, JSValue this_val) {
  auto* shadow = unwrap_shadow_root(this_val);
  if (!shadow) return JS_NewString(ctx, "open");
  return JS_NewString(ctx, shadow->mode == LXB_DOM_SHADOW_ROOT_MODE_CLOSED ? "closed" : "open");
}

JSValue js_shadow_get_innerHTML(JSContext* ctx, JSValue this_val) {
  auto* shadow = unwrap_shadow_root(this_val);
  if (!shadow) return JS_NewString(ctx, "");
  std::string result;
  lxb_dom_node_t* child = lxb_dom_interface_node(shadow)->first_child;
  while (child) { result += serialize_node(child); child = child->next; }
  return JS_NewString(ctx, result.c_str());
}

JSValue js_shadow_set_innerHTML(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* shadow = unwrap_shadow_root(this_val);
  if (!shadow) return JS_UNDEFINED;
  const char* html = JS_ToCString(ctx, val);
  if (!html) return JS_UNDEFINED;

  lxb_dom_node_t* node = lxb_dom_interface_node(shadow);
  auto* rctx = get_ctx(ctx);
  std::vector<void*> old_children;
  lxb_dom_node_t* child = node->first_child;
  while (child) { old_children.push_back(child); child = child->next; }
  if (!old_children.empty()) invalidate_node_cache_deep_batch(ctx, rctx, old_children);

  get_doc(ctx)->setInnerHTMLOnShadowRoot(shadow, shadow->host, html);
  JS_FreeCString(ctx, html);
  return JS_UNDEFINED;
}

JSValue js_shadow_querySelector(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* shadow = unwrap_shadow_root(this_val);
  if (!shadow || argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  void* found = get_doc(ctx)->querySelectorFromEl(shadow, sel);
  JS_FreeCString(ctx, sel);
  return make_element(ctx, found);
}

JSValue js_shadow_querySelectorAll(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* shadow = unwrap_shadow_root(this_val);
  if (!shadow || argc < 1) return JS_NewArray(ctx);
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto found = get_doc(ctx)->querySelectorAllFromEl(shadow, sel);
  JS_FreeCString(ctx, sel);
  return make_element_array(ctx, found);
}

// ── Element.prototype.attachShadow() / .shadowRoot ────────────────────────────

JSValue js_el_attachShadow(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  if (argc < 1 || !JS_IsObject(argv[0])) {
    return JS_ThrowTypeError(ctx, "attachShadow() requires an options object with a 'mode' property");
  }
  JSValue mode_val = JS_GetPropertyStr(ctx, argv[0], "mode");
  if (JS_IsException(mode_val)) return JS_EXCEPTION;
  const char* mode_str = JS_ToCString(ctx, mode_val);
  JS_FreeValue(ctx, mode_val);
  if (!mode_str) return JS_EXCEPTION;
  bool is_open = strcmp(mode_str, "open") == 0;
  bool is_closed = strcmp(mode_str, "closed") == 0;
  JS_FreeCString(ctx, mode_str);
  if (!is_open && !is_closed) {
    return JS_ThrowTypeError(ctx, "attachShadow() options.mode must be 'open' or 'closed'");
  }

  auto* rctx = get_ctx(ctx);
  if (rctx && rctx->element_shadow_roots.count(el)) {
    return throw_dom_exception(ctx, "NotSupportedError", "This element already has an attached shadow root.");
  }

  void* shadow = get_doc(ctx)->createShadowRoot(el, is_closed ? 1 : 0);
  if (!shadow) return JS_NULL;
  if (rctx) rctx->element_shadow_roots[el] = shadow;

  return make_shadow_root(ctx, shadow);
}

JSValue js_el_get_shadowRoot(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_NULL;
  auto it = rctx->element_shadow_roots.find(el);
  if (it == rctx->element_shadow_roots.end()) return JS_NULL;
  auto* shadow = static_cast<lxb_dom_shadow_root_t*>(it->second);
  if (shadow->mode == LXB_DOM_SHADOW_ROOT_MODE_CLOSED) return JS_NULL;
  return make_shadow_root(ctx, shadow);
}

} // namespace

void ShadowRootBindings::install(JSContext* ctx) {
  if (js_shadow_root_class_id == 0) JS_NewClassID(&js_shadow_root_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_shadow_root_class_id, &js_shadow_root_class);

  JSValue node_proto = JS_GetClassProto(ctx, js_node_class_id);
  JSValue proto = JS_NewObjectProto(ctx, node_proto);
  JS_FreeValue(ctx, node_proto);

  define_prop(ctx, proto, "host",       js_shadow_get_host,       nullptr);
  define_prop(ctx, proto, "mode",       js_shadow_get_mode,       nullptr);
  define_prop(ctx, proto, "innerHTML",  js_shadow_get_innerHTML,  js_shadow_set_innerHTML);
  JS_SetPropertyStr(ctx, proto, "querySelector",
      JS_NewCFunction(ctx, js_shadow_querySelector, "querySelector", 1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll",
      JS_NewCFunction(ctx, js_shadow_querySelectorAll, "querySelectorAll", 1));

  JS_SetClassProto(ctx, js_shadow_root_class_id, proto);
  JSValue shadow_proto_ref = JS_GetClassProto(ctx, js_shadow_root_class_id);
  JS_FreeValue(ctx, define_global_constructor(ctx, "ShadowRoot", shadow_proto_ref));
  JS_FreeValue(ctx, shadow_proto_ref);

  JSValue element_proto = JS_GetClassProto(ctx, js_element_class_id);
  JS_SetPropertyStr(ctx, element_proto, "attachShadow",
      JS_NewCFunction(ctx, js_el_attachShadow, "attachShadow", 1));
  define_prop(ctx, element_proto, "shadowRoot", js_el_get_shadowRoot, nullptr);
  JS_FreeValue(ctx, element_proto);
}

} // namespace margelo::nitro::nitrojsdom
