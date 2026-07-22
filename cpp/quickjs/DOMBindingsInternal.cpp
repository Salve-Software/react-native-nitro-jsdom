#include "DOMBindingsInternal.hpp"
#include "QuickJSRuntime.hpp"
#include "../lexbor/LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <cctype>
#include <sstream>

namespace margelo::nitro::nitrojsdom {

JSClassID js_element_class_id     = 0;
JSClassID js_node_class_id        = 0;
JSClassID js_shadow_root_class_id = 0;

namespace {
JSClassDef js_element_class = { "Element", .finalizer = nullptr };
} // namespace

JSValue make_element(JSContext* ctx, void* ptr) {
  if (!ptr) return JS_NULL;

  RuntimeContext* rctx = get_ctx(ctx);
  if (rctx) {
    auto it = rctx->node_wrapper_cache.find(ptr);
    if (it != rctx->node_wrapper_cache.end()) {
      JSValue* cached = static_cast<JSValue*>(it->second);
      return JS_DupValue(ctx, *cached);
    }
  }

  lxb_dom_node_t* node = static_cast<lxb_dom_node_t*>(ptr);
  JSValue obj;
  if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    obj = JS_NewObjectClass(ctx, js_element_class_id);
  } else {
    obj = JS_NewObjectClass(ctx, js_node_class_id);
  }
  JS_SetOpaque(obj, ptr);

  if (rctx) {
    rctx->node_wrapper_cache[ptr] = new JSValue(JS_DupValue(ctx, obj));
  }

  return obj;
}

void invalidate_node_cache(JSContext* ctx, RuntimeContext* rctx, void* node) {
  if (!rctx || !node) return;
  auto it = rctx->node_wrapper_cache.find(node);
  if (it != rctx->node_wrapper_cache.end()) {
    JSValue* v = static_cast<JSValue*>(it->second);
    JS_SetOpaque(*v, nullptr);
    JS_FreeValue(ctx, *v);
    delete v;
    rctx->node_wrapper_cache.erase(it);
  }
}

void invalidate_node_cache_batch(JSContext* ctx, RuntimeContext* rctx, const std::vector<void*>& nodes) {
  for (void* n : nodes) invalidate_node_cache(ctx, rctx, n);
}

namespace {
void invalidate_node_cache_deep(JSContext* ctx, RuntimeContext* rctx, void* node_ptr) {
  if (!node_ptr) return;
  invalidate_node_cache(ctx, rctx, node_ptr);
  for (lxb_dom_node_t* child = static_cast<lxb_dom_node_t*>(node_ptr)->first_child;
       child; child = child->next) {
    invalidate_node_cache_deep(ctx, rctx, child);
  }
}
} // namespace

void invalidate_node_cache_deep_batch(JSContext* ctx, RuntimeContext* rctx, const std::vector<void*>& roots) {
  if (!rctx) return;
  for (void* root : roots) invalidate_node_cache_deep(ctx, rctx, root);
}

void clear_node_cache(JSContext* ctx, RuntimeContext* rctx) {
  if (!rctx) return;
  for (auto& kv : rctx->node_wrapper_cache) {
    JSValue* v = static_cast<JSValue*>(kv.second);
    JS_SetOpaque(*v, nullptr);
    JS_FreeValue(ctx, *v);
    delete v;
  }
  rctx->node_wrapper_cache.clear();
}

JSValue make_element_array(JSContext* ctx, const std::vector<void*>& elements) {
  JSValue arr = JS_NewArray(ctx);
  for (size_t i = 0; i < elements.size(); i++)
    JS_SetPropertyUint32(ctx, arr, (uint32_t)i, make_element(ctx, elements[i]));
  return arr;
}

lxb_dom_element_t* unwrap_element(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_element_class_id));
}

lxb_dom_node_t* unwrap_node(JSContext* ctx, JSValue val) {
  void* p = JS_GetOpaque(val, js_element_class_id);
  if (p) return static_cast<lxb_dom_node_t*>(p);
  p = JS_GetOpaque(val, js_node_class_id);
  if (p) return static_cast<lxb_dom_node_t*>(p);
  p = JS_GetOpaque(val, js_shadow_root_class_id);
  return static_cast<lxb_dom_node_t*>(p);
}

std::string serialize_node(lxb_dom_node_t* node) {
  std::string result;
  lxb_html_serialize_tree_cb(node,
    [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
      static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
      return LXB_STATUS_OK;
    }, &result);
  return result;
}

RuntimeContext* get_ctx(JSContext* ctx) {
  return static_cast<RuntimeContext*>(JS_GetContextOpaque(ctx));
}

LexborDocument* get_doc(JSContext* ctx) {
  auto* rctx = get_ctx(ctx);
  return rctx ? rctx->document : nullptr;
}

void define_prop(JSContext* ctx, JSValue obj, const char* name, GetterFn getter, SetterFn setter) {
  JSAtom atom = JS_NewAtom(ctx, name);
  JSValue get_fn = JS_NewCFunction2(ctx, (JSCFunction*)getter, name, 0, JS_CFUNC_getter, 0);
  JSValue set_fn = setter
      ? JS_NewCFunction2(ctx, (JSCFunction*)setter, name, 1, JS_CFUNC_setter, 0)
      : JS_UNDEFINED;
  JS_DefinePropertyGetSet(ctx, obj, atom, get_fn, set_fn, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, atom);
}

JSValue js_illegal_constructor(JSContext* ctx, JSValue, int, JSValue*) {
  return JS_ThrowTypeError(ctx, "Illegal constructor");
}

JSValue define_global_constructor(JSContext* ctx, const char* name, JSValue proto) {
  JSValue ctor = JS_NewCFunction2(ctx, js_illegal_constructor, name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor(ctx, ctor, proto);
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, name, JS_DupValue(ctx, ctor));
  JS_FreeValue(ctx, global);
  return ctor;
}

bool get_bool_prop(JSContext* ctx, JSValue obj, const char* name) {
  JSValue v = JS_GetPropertyStr(ctx, obj, name);
  bool b = JS_ToBool(ctx, v) > 0;
  JS_FreeValue(ctx, v);
  return b;
}

std::string classNames_to_selector(const std::string& names) {
  std::string sel;
  std::istringstream iss(names);
  std::string cls;
  while (iss >> cls) { sel += '.'; sel += cls; }
  return sel;
}

// data-foo-bar -> fooBar ; background-color -> backgroundColor
std::string attr_suffix_to_camel(const std::string& suffix) {
  std::string out;
  bool upper_next = false;
  for (char c : suffix) {
    if (c == '-') { upper_next = true; continue; }
    out += upper_next ? (char)std::toupper((unsigned char)c) : c;
    upper_next = false;
  }
  return out;
}

// fooBar -> foo-bar ; backgroundColor -> background-color
std::string camel_to_attr_suffix(const std::string& camel) {
  std::string out;
  for (char c : camel) {
    if (std::isupper((unsigned char)c)) { out += '-'; out += (char)std::tolower((unsigned char)c); }
    else out += c;
  }
  return out;
}

} // namespace margelo::nitro::nitrojsdom
