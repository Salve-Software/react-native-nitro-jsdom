#include "DOMBindingsInternal.hpp"
#include "QuickJSRuntime.hpp"
#include "../lexbor/LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <cctype>
#include <sstream>

namespace margelo::nitro::nitrojsdom {

JSClassID js_element_class_id = 0; // non-static: shared via this header

namespace {
JSClassDef js_element_class = { "Element", .finalizer = nullptr };
} // namespace

JSValue make_element(JSContext* ctx, void* el) {
  if (!el) return JS_NULL;
  JSValue obj = JS_NewObjectClass(ctx, js_element_class_id);
  JS_SetOpaque(obj, el);
  return obj;
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
