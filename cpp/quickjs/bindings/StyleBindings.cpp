#include "StyleBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include <lexbor/dom/dom.h>
#include <algorithm>
#include <utility>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_style_class_id = 0;

lxb_dom_element_t* unwrap_style(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_style_class_id));
}

std::string get_style_attr(lxb_dom_element_t* el) {
  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>("style"), 5, &len);
  return val ? std::string(reinterpret_cast<const char*>(val), len) : "";
}

void set_style_attr(lxb_dom_element_t* el, const std::string& css) {
  if (css.empty()) {
    lxb_dom_element_remove_attribute(el, reinterpret_cast<const lxb_char_t*>("style"), 5);
  } else {
    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>("style"), 5,
        reinterpret_cast<const lxb_char_t*>(css.data()), css.size());
  }
}

std::string trim_ws(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\n\r");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\n\r");
  return s.substr(a, b - a + 1);
}

// Parses "color: red; font-size: 12px" into ordered (name, value) pairs.
std::vector<std::pair<std::string, std::string>> parse_style_decls(const std::string& css) {
  std::vector<std::pair<std::string, std::string>> decls;
  size_t pos = 0;
  while (pos < css.size()) {
    size_t semi = css.find(';', pos);
    std::string decl = css.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
    pos = (semi == std::string::npos) ? css.size() : semi + 1;

    size_t colon = decl.find(':');
    if (colon == std::string::npos) continue;
    std::string name = trim_ws(decl.substr(0, colon));
    std::string value = trim_ws(decl.substr(colon + 1));
    if (!name.empty()) decls.emplace_back(name, value);
  }
  return decls;
}

std::string serialize_style_decls(const std::vector<std::pair<std::string, std::string>>& decls) {
  std::string out;
  for (auto& kv : decls) { out += kv.first; out += ": "; out += kv.second; out += "; "; }
  if (!out.empty()) out.pop_back();
  return out;
}

int js_style_get_own_property(JSContext* ctx, JSPropertyDescriptor* desc, JSValue obj, JSAtom prop) {
  auto* el = unwrap_style(ctx, obj);
  if (!el) return 0;
  JSValue key_val = JS_AtomToString(ctx, prop);
  const char* key = JS_ToCString(ctx, key_val);
  JS_FreeValue(ctx, key_val);
  if (!key) return 0;
  std::string prop_name = camel_to_attr_suffix(key);
  JS_FreeCString(ctx, key);

  auto decls = parse_style_decls(get_style_attr(el));
  for (auto& kv : decls) {
    if (kv.first == prop_name) {
      if (desc) {
        desc->flags = JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
        desc->value = JS_NewStringLen(ctx, kv.second.data(), kv.second.size());
        desc->getter = JS_UNDEFINED;
        desc->setter = JS_UNDEFINED;
      }
      return 1;
    }
  }
  return 0;
}

int js_style_define_own_property(JSContext* ctx, JSValue this_obj, JSAtom prop, JSValue val,
                                  JSValue, JSValue, int) {
  auto* el = unwrap_style(ctx, this_obj);
  if (!el) return 0;
  JSValue key_val = JS_AtomToString(ctx, prop);
  const char* key = JS_ToCString(ctx, key_val);
  JS_FreeValue(ctx, key_val);
  if (!key) return 0;
  std::string prop_name = camel_to_attr_suffix(key);
  JS_FreeCString(ctx, key);

  const char* str = JS_ToCString(ctx, val);
  if (str) {
    auto decls = parse_style_decls(get_style_attr(el));
    bool found = false;
    for (auto& kv : decls) { if (kv.first == prop_name) { kv.second = str; found = true; break; } }
    if (!found) decls.emplace_back(prop_name, str);
    set_style_attr(el, serialize_style_decls(decls));
    JS_FreeCString(ctx, str);
  }
  return 1;
}

int js_style_delete_property(JSContext* ctx, JSValue obj, JSAtom prop) {
  auto* el = unwrap_style(ctx, obj);
  if (!el) return 1;
  JSValue key_val = JS_AtomToString(ctx, prop);
  const char* key = JS_ToCString(ctx, key_val);
  JS_FreeValue(ctx, key_val);
  if (!key) return 1;
  std::string prop_name = camel_to_attr_suffix(key);
  JS_FreeCString(ctx, key);

  auto decls = parse_style_decls(get_style_attr(el));
  decls.erase(std::remove_if(decls.begin(), decls.end(),
      [&](auto& kv) { return kv.first == prop_name; }), decls.end());
  set_style_attr(el, serialize_style_decls(decls));
  return 1;
}

int js_style_get_own_property_names(JSContext* ctx, JSPropertyEnum** ptab, uint32_t* plen, JSValue obj) {
  *ptab = nullptr;
  *plen = 0;
  auto* el = unwrap_style(ctx, obj);
  if (!el) return 0;
  auto decls = parse_style_decls(get_style_attr(el));

  auto* tab = static_cast<JSPropertyEnum*>(js_malloc(ctx, sizeof(JSPropertyEnum) * (decls.empty() ? 1 : decls.size())));
  if (!tab) return -1;
  for (size_t i = 0; i < decls.size(); i++) {
    tab[i].is_enumerable = 1;
    tab[i].atom = JS_NewAtom(ctx, attr_suffix_to_camel(decls[i].first).c_str());
  }
  *ptab = tab;
  *plen = (uint32_t)decls.size();
  return 0;
}

JSClassExoticMethods js_style_exotic = {
  .get_own_property       = js_style_get_own_property,
  .get_own_property_names = js_style_get_own_property_names,
  .delete_property        = js_style_delete_property,
  .define_own_property    = js_style_define_own_property,
};

JSClassDef js_style_class = { "CSSStyleDeclaration", .finalizer = nullptr, .exotic = &js_style_exotic };

JSValue js_style_get_cssText(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  return JS_NewString(ctx, get_style_attr(el).c_str());
}

JSValue js_style_set_cssText(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (str) { set_style_attr(el, str); JS_FreeCString(ctx, str); }
  return JS_UNDEFINED;
}

JSValue js_style_setProperty(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el || argc < 2) return JS_UNDEFINED;
  const char* name  = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);
  if (name && value) {
    auto decls = parse_style_decls(get_style_attr(el));
    bool found = false;
    for (auto& kv : decls) { if (kv.first == name) { kv.second = value; found = true; break; } }
    if (!found) decls.emplace_back(name, value);
    set_style_attr(el, serialize_style_decls(decls));
  }
  if (name)  JS_FreeCString(ctx, name);
  if (value) JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

JSValue js_style_getPropertyValue(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el || argc < 1) return JS_NewString(ctx, "");
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_NewString(ctx, "");
  auto decls = parse_style_decls(get_style_attr(el));
  std::string result;
  for (auto& kv : decls) { if (kv.first == name) { result = kv.second; break; } }
  JS_FreeCString(ctx, name);
  return JS_NewString(ctx, result.c_str());
}

JSValue js_style_removeProperty(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el || argc < 1) return JS_NewString(ctx, "");
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_NewString(ctx, "");
  auto decls = parse_style_decls(get_style_attr(el));
  std::string removed;
  decls.erase(std::remove_if(decls.begin(), decls.end(), [&](auto& kv) {
    if (kv.first == name) { removed = kv.second; return true; }
    return false;
  }), decls.end());
  set_style_attr(el, serialize_style_decls(decls));
  JS_FreeCString(ctx, name);
  return JS_NewString(ctx, removed.c_str());
}

} // namespace

void StyleBindings::install(JSContext* ctx) {
  if (js_style_class_id == 0) JS_NewClassID(&js_style_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_style_class_id, &js_style_class);

  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, proto, "setProperty",      JS_NewCFunction(ctx, js_style_setProperty,      "setProperty",      2));
  JS_SetPropertyStr(ctx, proto, "getPropertyValue", JS_NewCFunction(ctx, js_style_getPropertyValue, "getPropertyValue", 1));
  JS_SetPropertyStr(ctx, proto, "removeProperty",   JS_NewCFunction(ctx, js_style_removeProperty,   "removeProperty",   1));
  define_prop(ctx, proto, "cssText", js_style_get_cssText, js_style_set_cssText);
  JS_SetClassProto(ctx, js_style_class_id, proto);
}

JSValue StyleBindings::make(JSContext* ctx, lxb_dom_element_t* el) {
  JSValue obj = JS_NewObjectClass(ctx, js_style_class_id);
  JS_SetOpaque(obj, el);
  return obj;
}

} // namespace margelo::nitro::nitrojsdom
