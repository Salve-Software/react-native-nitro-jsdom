#include "DOMBindings.hpp"
#include "MutationObservers.hpp"
#include "Storage.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "QuickJSRuntime.hpp"
#include "quickjs.h"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <lexbor/dom/interfaces/character_data.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <functional>
#include <chrono>
#include <cstring>
#include <optional>

namespace margelo::nitro::nitrojsdom {

// ── Class IDs ────────────────────────────────────────────────────────────────

JSClassID js_element_class_id   = 0;  // non-static: shared via DOMBindingsInternal.hpp
static JSClassID js_classList_class_id = 0;

static JSClassDef js_element_class   = { "Element",      .finalizer = nullptr };
static JSClassDef js_classList_class = { "DOMTokenList", .finalizer = nullptr };

// ── Core helpers ──────────────────────────────────────────────────────────────

static RuntimeContext* get_ctx(JSContext* ctx) {
  return static_cast<RuntimeContext*>(JS_GetContextOpaque(ctx));
}

static LexborDocument* get_doc(JSContext* ctx) {
  auto* rctx = get_ctx(ctx);
  return rctx ? rctx->document : nullptr;
}

static lxb_dom_element_t* unwrap_element(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_element_class_id));
}

JSValue make_element(JSContext* ctx, void* el) {
  if (!el) return JS_NULL;
  JSValue obj = JS_NewObjectClass(ctx, js_element_class_id);
  JS_SetOpaque(obj, el);
  return obj;
}

static JSValue make_element_array(JSContext* ctx, const std::vector<void*>& elements) {
  JSValue arr = JS_NewArray(ctx);
  for (size_t i = 0; i < elements.size(); i++)
    JS_SetPropertyUint32(ctx, arr, (uint32_t)i, make_element(ctx, elements[i]));
  return arr;
}

static std::string serialize_node(lxb_dom_node_t* node) {
  std::string result;
  lxb_html_serialize_tree_cb(node,
    [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
      static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
      return LXB_STATUS_OK;
    }, &result);
  return result;
}

// ── classList helpers ─────────────────────────────────────────────────────────

static std::string get_class_attr(lxb_dom_element_t* el) {
  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>("class"), 5, &len);
  return val ? std::string(reinterpret_cast<const char*>(val), len) : "";
}

static void set_class_attr(lxb_dom_element_t* el, const std::string& classes) {
  if (classes.empty()) {
    lxb_dom_element_remove_attribute(el,
        reinterpret_cast<const lxb_char_t*>("class"), 5);
  } else {
    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>("class"), 5,
        reinterpret_cast<const lxb_char_t*>(classes.data()), classes.size());
  }
}

static std::vector<std::string> split_classes(const std::string& str) {
  std::vector<std::string> result;
  std::istringstream iss(str);
  std::string token;
  while (iss >> token) result.push_back(token);
  return result;
}

static std::string join_classes(const std::vector<std::string>& v) {
  std::string r;
  for (size_t i = 0; i < v.size(); i++) { if (i) r += ' '; r += v[i]; }
  return r;
}

static std::string classNames_to_selector(const std::string& names) {
  std::string sel;
  for (const auto& cls : split_classes(names)) { sel += '.'; sel += cls; }
  return sel;
}

// ── classList methods ─────────────────────────────────────────────────────────

static lxb_dom_element_t* unwrap_classList(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_classList_class_id));
}

static JSValue js_classList_add(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  auto classes = split_classes(get_class_attr(el));
  for (int i = 0; i < argc; i++) {
    const char* cls = JS_ToCString(ctx, argv[i]);
    if (cls) {
      if (std::find(classes.begin(), classes.end(), cls) == classes.end()) classes.push_back(cls);
      JS_FreeCString(ctx, cls);
    }
  }
  set_class_attr(el, join_classes(classes));

  if (has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
        "class", old_val);
  }
  return JS_UNDEFINED;
}

static JSValue js_classList_remove(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  auto classes = split_classes(get_class_attr(el));
  for (int i = 0; i < argc; i++) {
    const char* cls = JS_ToCString(ctx, argv[i]);
    if (cls) { classes.erase(std::remove(classes.begin(), classes.end(), cls), classes.end()); JS_FreeCString(ctx, cls); }
  }
  set_class_attr(el, join_classes(classes));

  if (has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
        "class", old_val);
  }
  return JS_UNDEFINED;
}

static JSValue js_classList_contains(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* cls = JS_ToCString(ctx, argv[0]);
  if (!cls) return JS_FALSE;
  bool found = std::find(split_classes(get_class_attr(el)).begin(),
                         split_classes(get_class_attr(el)).end(), cls) != split_classes(get_class_attr(el)).end();
  JS_FreeCString(ctx, cls);
  return JS_NewBool(ctx, found);
}

static JSValue js_classList_toggle(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* cls = JS_ToCString(ctx, argv[0]);
  if (!cls) return JS_FALSE;

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  auto classes = split_classes(get_class_attr(el));
  auto it = std::find(classes.begin(), classes.end(), cls);
  bool added;
  if (it != classes.end()) { classes.erase(it); added = false; }
  else { classes.push_back(cls); added = true; }
  JS_FreeCString(ctx, cls);
  set_class_attr(el, join_classes(classes));

  if (has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
        "class", old_val);
  }
  return JS_NewBool(ctx, added);
}

static JSValue js_classList_replace(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 2) return JS_FALSE;
  const char* oldCls = JS_ToCString(ctx, argv[0]);
  const char* newCls = JS_ToCString(ctx, argv[1]);
  bool replaced = false;
  if (oldCls && newCls) {
    auto* rctx = get_ctx(ctx);
    bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
    std::optional<std::string> old_val;
    if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
      old_val = get_class_attr(el);

    auto classes = split_classes(get_class_attr(el));
    auto it = std::find(classes.begin(), classes.end(), oldCls);
    if (it != classes.end()) {
      *it = newCls; replaced = true; set_class_attr(el, join_classes(classes));
      if (has_obs) {
        rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
            "class", old_val);
      }
    }
  }
  if (oldCls) JS_FreeCString(ctx, oldCls);
  if (newCls) JS_FreeCString(ctx, newCls);
  return JS_NewBool(ctx, replaced);
}

static JSValue make_classList(JSContext* ctx, lxb_dom_element_t* el) {
  JSValue obj = JS_NewObjectClass(ctx, js_classList_class_id);
  JS_SetOpaque(obj, el);
  JS_SetPropertyStr(ctx, obj, "add",      JS_NewCFunction(ctx, js_classList_add,      "add",      1));
  JS_SetPropertyStr(ctx, obj, "remove",   JS_NewCFunction(ctx, js_classList_remove,   "remove",   1));
  JS_SetPropertyStr(ctx, obj, "contains", JS_NewCFunction(ctx, js_classList_contains, "contains", 1));
  JS_SetPropertyStr(ctx, obj, "toggle",   JS_NewCFunction(ctx, js_classList_toggle,   "toggle",   1));
  JS_SetPropertyStr(ctx, obj, "replace",  JS_NewCFunction(ctx, js_classList_replace,  "replace",  2));
  return obj;
}

// ── dataset (DOMStringMap, mirrors data-* attributes) ─────────────────────────

static JSClassID js_dataset_class_id = 0;

static lxb_dom_element_t* unwrap_dataset(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_dataset_class_id));
}

// data-foo-bar -> fooBar
static std::string attr_suffix_to_camel(const std::string& suffix) {
  std::string out;
  bool upper_next = false;
  for (char c : suffix) {
    if (c == '-') { upper_next = true; continue; }
    out += upper_next ? (char)std::toupper((unsigned char)c) : c;
    upper_next = false;
  }
  return out;
}

// fooBar -> foo-bar
static std::string camel_to_attr_suffix(const std::string& camel) {
  std::string out;
  for (char c : camel) {
    if (std::isupper((unsigned char)c)) { out += '-'; out += (char)std::tolower((unsigned char)c); }
    else out += c;
  }
  return out;
}

static std::string dataset_prop_to_attr(JSContext* ctx, JSAtom prop) {
  JSValue key_val = JS_AtomToString(ctx, prop);
  const char* key = JS_ToCString(ctx, key_val);
  JS_FreeValue(ctx, key_val);
  std::string attr_name = key ? ("data-" + camel_to_attr_suffix(key)) : "";
  if (key) JS_FreeCString(ctx, key);
  return attr_name;
}

static int js_dataset_get_own_property(JSContext* ctx, JSPropertyDescriptor* desc, JSValue obj, JSAtom prop) {
  auto* el = unwrap_dataset(ctx, obj);
  if (!el) return 0;
  std::string attr_name = dataset_prop_to_attr(ctx, prop);
  if (attr_name.empty()) return 0;

  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>(attr_name.data()), attr_name.size(), &len);
  if (!val) return 0;

  if (desc) {
    desc->flags = JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
    desc->value = JS_NewStringLen(ctx, reinterpret_cast<const char*>(val), len);
    desc->getter = JS_UNDEFINED;
    desc->setter = JS_UNDEFINED;
  }
  return 1;
}

static int js_dataset_define_own_property(JSContext* ctx, JSValue this_obj, JSAtom prop, JSValue val,
                                           JSValue, JSValue, int) {
  auto* el = unwrap_dataset(ctx, this_obj);
  if (!el) return 0;
  std::string attr_name = dataset_prop_to_attr(ctx, prop);
  if (attr_name.empty()) return 0;

  const char* str = JS_ToCString(ctx, val);
  if (str) {
    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>(attr_name.data()), attr_name.size(),
        reinterpret_cast<const lxb_char_t*>(str), strlen(str));
    JS_FreeCString(ctx, str);
  }
  return 1;
}

static int js_dataset_delete_property(JSContext* ctx, JSValue obj, JSAtom prop) {
  auto* el = unwrap_dataset(ctx, obj);
  if (!el) return 1;
  std::string attr_name = dataset_prop_to_attr(ctx, prop);
  if (!attr_name.empty()) {
    lxb_dom_element_remove_attribute(el,
        reinterpret_cast<const lxb_char_t*>(attr_name.data()), attr_name.size());
  }
  return 1;
}

static int js_dataset_get_own_property_names(JSContext* ctx, JSPropertyEnum** ptab, uint32_t* plen, JSValue obj) {
  *ptab = nullptr;
  *plen = 0;
  auto* el = unwrap_dataset(ctx, obj);
  if (!el) return 0;

  std::vector<std::string> keys;
  for (lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(el); attr; attr = lxb_dom_element_next_attribute(attr)) {
    size_t len = 0;
    const lxb_char_t* name = lxb_dom_attr_qualified_name(attr, &len);
    std::string n(reinterpret_cast<const char*>(name), len);
    if (n.rfind("data-", 0) == 0) keys.push_back(attr_suffix_to_camel(n.substr(5)));
  }

  auto* tab = static_cast<JSPropertyEnum*>(js_malloc(ctx, sizeof(JSPropertyEnum) * (keys.empty() ? 1 : keys.size())));
  if (!tab) return -1;
  for (size_t i = 0; i < keys.size(); i++) {
    tab[i].is_enumerable = 1;
    tab[i].atom = JS_NewAtom(ctx, keys[i].c_str());
  }
  *ptab = tab;
  *plen = (uint32_t)keys.size();
  return 0;
}

static JSClassExoticMethods js_dataset_exotic = {
  .get_own_property       = js_dataset_get_own_property,
  .get_own_property_names = js_dataset_get_own_property_names,
  .delete_property        = js_dataset_delete_property,
  .define_own_property    = js_dataset_define_own_property,
};

static JSClassDef js_dataset_class = { "DOMStringMap", .finalizer = nullptr, .exotic = &js_dataset_exotic };

static JSValue make_dataset(JSContext* ctx, lxb_dom_element_t* el) {
  JSValue obj = JS_NewObjectClass(ctx, js_dataset_class_id);
  JS_SetOpaque(obj, el);
  return obj;
}

// ── style (CSSStyleDeclaration-like, mirrors the `style` attribute) ──────────

static JSClassID js_style_class_id = 0;

static lxb_dom_element_t* unwrap_style(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_style_class_id));
}

static std::string get_style_attr(lxb_dom_element_t* el) {
  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>("style"), 5, &len);
  return val ? std::string(reinterpret_cast<const char*>(val), len) : "";
}

static void set_style_attr(lxb_dom_element_t* el, const std::string& css) {
  if (css.empty()) {
    lxb_dom_element_remove_attribute(el, reinterpret_cast<const lxb_char_t*>("style"), 5);
  } else {
    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>("style"), 5,
        reinterpret_cast<const lxb_char_t*>(css.data()), css.size());
  }
}

static std::string trim_ws(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\n\r");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\n\r");
  return s.substr(a, b - a + 1);
}

static std::vector<std::pair<std::string, std::string>> parse_style_decls(const std::string& css) {
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

static std::string serialize_style_decls(const std::vector<std::pair<std::string, std::string>>& decls) {
  std::string out;
  for (auto& kv : decls) { out += kv.first; out += ": "; out += kv.second; out += "; "; }
  if (!out.empty()) out.pop_back();
  return out;
}

static int js_style_get_own_property(JSContext* ctx, JSPropertyDescriptor* desc, JSValue obj, JSAtom prop) {
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

static int js_style_define_own_property(JSContext* ctx, JSValue this_obj, JSAtom prop, JSValue val,
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

static int js_style_delete_property(JSContext* ctx, JSValue obj, JSAtom prop) {
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

static int js_style_get_own_property_names(JSContext* ctx, JSPropertyEnum** ptab, uint32_t* plen, JSValue obj) {
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

static JSClassExoticMethods js_style_exotic = {
  .get_own_property       = js_style_get_own_property,
  .get_own_property_names = js_style_get_own_property_names,
  .delete_property        = js_style_delete_property,
  .define_own_property    = js_style_define_own_property,
};

static JSClassDef js_style_class = { "CSSStyleDeclaration", .finalizer = nullptr, .exotic = &js_style_exotic };

static JSValue js_style_get_cssText(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  return JS_NewString(ctx, get_style_attr(el).c_str());
}

static JSValue js_style_set_cssText(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_style(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (str) { set_style_attr(el, str); JS_FreeCString(ctx, str); }
  return JS_UNDEFINED;
}

static JSValue js_style_setProperty(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

static JSValue js_style_getPropertyValue(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

static JSValue js_style_removeProperty(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

static JSValue make_style(JSContext* ctx, lxb_dom_element_t* el) {
  JSValue obj = JS_NewObjectClass(ctx, js_style_class_id);
  JS_SetOpaque(obj, el);
  return obj;
}

// ── Element property getters/setters ─────────────────────────────────────────

static JSValue js_el_get_tagName(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  size_t len = 0;
  const lxb_char_t* name = lxb_dom_element_local_name(el, &len);
  if (!name || len == 0) return JS_NewString(ctx, "");
  std::string r(reinterpret_cast<const char*>(name), len);
  for (auto& c : r) c = (char)std::toupper((unsigned char)c);
  return JS_NewStringLen(ctx, r.c_str(), r.size());
}

static JSValue js_el_get_id(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>("id"), 2, &len);
  return val ? JS_NewStringLen(ctx, reinterpret_cast<const char*>(val), len) : JS_NewString(ctx, "");
}

static JSValue js_el_set_id(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (str) {
    auto* rctx = get_ctx(ctx);
    bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();

    std::optional<std::string> old_val;
    if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver()) {
      size_t len = 0;
      const lxb_char_t* v = lxb_dom_element_get_attribute(el,
          reinterpret_cast<const lxb_char_t*>("id"), 2, &len);
      if (v) old_val = std::string(reinterpret_cast<const char*>(v), len);
    }

    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>("id"), 2,
        reinterpret_cast<const lxb_char_t*>(str), strlen(str));
    JS_FreeCString(ctx, str);

    if (has_obs) {
      rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
          "id", old_val);
    }
  }
  return JS_UNDEFINED;
}

static JSValue js_el_get_className(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  return JS_NewString(ctx, get_class_attr(el).c_str());
}

static JSValue js_el_set_className(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (str) {
    auto* rctx = get_ctx(ctx);
    bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
    std::optional<std::string> old_val;
    if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
      old_val = get_class_attr(el);

    set_class_attr(el, str);
    JS_FreeCString(ctx, str);

    if (has_obs) {
      rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
          "class", old_val);
    }
  }
  return JS_UNDEFINED;
}

static JSValue js_el_get_textContent(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  size_t len = 0;
  lxb_char_t* text = lxb_dom_node_text_content(node, &len);
  if (!text) return JS_NewString(ctx, "");
  JSValue result = JS_NewStringLen(ctx, reinterpret_cast<char*>(text), len);
  lxb_dom_document_destroy_text(node->owner_document, text);
  return result;
}

static JSValue js_el_set_textContent(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (!str) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_observers = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();

  lxb_dom_node_t* node = lxb_dom_interface_node(el);

  // Determine if this is an element node (childList) or text node (characterData)
  if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
    // characterData mutation: capture old value before mutation
    std::optional<std::string> old_val;
    if (has_observers && rctx->mutation_observers->hasCharacterDataOldValueObserver()) {
      size_t len = 0;
      lxb_char_t* text = lxb_dom_node_text_content(node, &len);
      if (text) {
        old_val = std::string(reinterpret_cast<char*>(text), len);
        lxb_dom_document_destroy_text(node->owner_document, text);
      }
    }
    get_doc(ctx)->setTextContentOnEl(el, str);
    if (has_observers) {
      rctx->mutation_observers->notifyCharacterData(ctx, node, old_val);
    }
  } else {
    // Element node: textContent replaces all children — fire childList
    // Collect old children before mutation
    std::vector<void*> old_children;
    if (has_observers) {
      lxb_dom_node_t* child = node->first_child;
      while (child) { old_children.push_back(child); child = child->next; }
    }
    // Disconnect observers targeting the old children before they are destroyed
    if (has_observers && !old_children.empty()) {
      rctx->mutation_observers->disconnectDetachedTargets(old_children);
    }
    get_doc(ctx)->setTextContentOnEl(el, str);
    if (has_observers) {
      // Collect new text node children after mutation
      std::vector<void*> new_children;
      lxb_dom_node_t* child = node->first_child;
      while (child) { new_children.push_back(child); child = child->next; }
      rctx->mutation_observers->notifyChildList(ctx, node,
          new_children, old_children, nullptr, nullptr);
    }
  }

  JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

static JSValue js_el_get_innerHTML(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  std::string result;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { result += serialize_node(child); child = child->next; }
  return JS_NewString(ctx, result.c_str());
}

static JSValue js_el_set_innerHTML(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  const char* html = JS_ToCString(ctx, val);
  if (!html) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_observers = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();

  lxb_dom_node_t* parent = lxb_dom_interface_node(el);

  // Collect old children before mutation
  std::vector<void*> old_children;
  if (has_observers) {
    lxb_dom_node_t* child = parent->first_child;
    while (child) { old_children.push_back(child); child = child->next; }
  }

  if (has_observers && !old_children.empty()) {
    rctx->mutation_observers->disconnectDetachedTargets(old_children);
  }

  get_doc(ctx)->setInnerHTMLOnEl(el, html);
  JS_FreeCString(ctx, html);

  if (has_observers) {
    std::vector<void*> new_children;
    lxb_dom_node_t* child = parent->first_child;
    while (child) { new_children.push_back(child); child = child->next; }
    rctx->mutation_observers->notifyChildList(ctx, parent,
        new_children, old_children, nullptr, nullptr);
  }

  return JS_UNDEFINED;
}

static JSValue js_el_get_outerHTML(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  return JS_NewString(ctx, serialize_node(lxb_dom_interface_node(el)).c_str());
}

static JSValue js_el_get_parentElement(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* parent = lxb_dom_interface_node(el)->parent;
  if (!parent || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_element(parent));
}

static JSValue js_el_get_children(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  JSValue arr = JS_NewArray(ctx);
  if (!el) return arr;
  uint32_t idx = 0;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) {
    if (child->type == LXB_DOM_NODE_TYPE_ELEMENT)
      JS_SetPropertyUint32(ctx, arr, idx++, make_element(ctx, lxb_dom_interface_element(child)));
    child = child->next;
  }
  return arr;
}

static JSValue js_el_get_firstElementChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->first_child;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->next;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

static JSValue js_el_get_lastElementChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->last_child;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->prev;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

static JSValue js_el_get_nextElementSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->next;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->next;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

static JSValue js_el_get_prevElementSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->prev;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->prev;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

static JSValue js_el_get_classList(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_classList(ctx, el);
}

static JSValue js_el_get_dataset(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_dataset(ctx, el);
}

static JSValue js_el_get_style(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_style(ctx, el);
}

static JSValue js_el_get_childElementCount(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewInt32(ctx, 0);
  int count = 0;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) count++; child = child->next; }
  return JS_NewInt32(ctx, count);
}

// ── Generic Node traversal getters ─────────────────────────────────

static JSValue js_el_get_nodeType(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewInt32(ctx, 0);
  return JS_NewInt32(ctx, (int32_t)lxb_dom_interface_node(el)->type);
}

static JSValue js_el_get_nodeName(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  switch (lxb_dom_interface_node(el)->type) {
    case LXB_DOM_NODE_TYPE_ELEMENT:           return js_el_get_tagName(ctx, this_val);
    case LXB_DOM_NODE_TYPE_TEXT:              return JS_NewString(ctx, "#text");
    case LXB_DOM_NODE_TYPE_COMMENT:           return JS_NewString(ctx, "#comment");
    case LXB_DOM_NODE_TYPE_DOCUMENT:          return JS_NewString(ctx, "#document");
    case LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT: return JS_NewString(ctx, "#document-fragment");
    default:                                  return JS_NewString(ctx, "");
  }
}

static JSValue js_el_get_nodeValue(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  if (node->type != LXB_DOM_NODE_TYPE_TEXT && node->type != LXB_DOM_NODE_TYPE_COMMENT) return JS_NULL;
  auto* cd = reinterpret_cast<lxb_dom_character_data_t*>(node);
  return JS_NewStringLen(ctx, reinterpret_cast<const char*>(cd->data.data), cd->data.length);
}

static JSValue js_el_set_nodeValue(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  if (node->type != LXB_DOM_NODE_TYPE_TEXT && node->type != LXB_DOM_NODE_TYPE_COMMENT) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (!str) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_observers = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  auto* cd = reinterpret_cast<lxb_dom_character_data_t*>(node);
  if (has_observers && rctx->mutation_observers->hasCharacterDataOldValueObserver())
    old_val = std::string(reinterpret_cast<const char*>(cd->data.data), cd->data.length);

  lxb_dom_character_data_replace(cd, reinterpret_cast<const lxb_char_t*>(str), strlen(str), 0, cd->data.length);
  JS_FreeCString(ctx, str);

  if (has_observers) rctx->mutation_observers->notifyCharacterData(ctx, node, old_val);
  return JS_UNDEFINED;
}

static JSValue js_el_get_childNodes(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  JSValue arr = JS_NewArray(ctx);
  if (!el) return arr;
  uint32_t idx = 0;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { JS_SetPropertyUint32(ctx, arr, idx++, make_element(ctx, child)); child = child->next; }
  return arr;
}

static JSValue js_el_get_firstChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->first_child);
}

static JSValue js_el_get_lastChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->last_child);
}

static JSValue js_el_get_nextSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->next);
}

static JSValue js_el_get_previousSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->prev);
}

static JSValue js_el_get_parentNode(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->parent);
}

// ── Element methods ───────────────────────────────────────────────────────────

static JSValue js_el_getAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NULL;
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_NULL;
  size_t val_len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>(name), strlen(name), &val_len);
  JS_FreeCString(ctx, name);
  return val ? JS_NewStringLen(ctx, reinterpret_cast<const char*>(val), val_len) : JS_NULL;
}

static JSValue js_el_setAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 2) return JS_UNDEFINED;
  const char* name  = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);
  if (name && value) {
    auto* rctx = get_ctx(ctx);
    bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();

    std::optional<std::string> old_val;
    if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver()) {
      size_t len = 0;
      const lxb_char_t* v = lxb_dom_element_get_attribute(el,
          reinterpret_cast<const lxb_char_t*>(name), strlen(name), &len);
      if (v) old_val = std::string(reinterpret_cast<const char*>(v), len);
    }

    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>(name),  strlen(name),
        reinterpret_cast<const lxb_char_t*>(value), strlen(value));

    if (has_obs) {
      rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
          std::string(name), old_val);
    }
  }
  if (name)  JS_FreeCString(ctx, name);
  if (value) JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

static JSValue js_el_removeAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_UNDEFINED;
  const char* name = JS_ToCString(ctx, argv[0]);
  if (name) {
    auto* rctx = get_ctx(ctx);
    bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();

    std::optional<std::string> old_val;
    if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver()) {
      size_t len = 0;
      const lxb_char_t* v = lxb_dom_element_get_attribute(el,
          reinterpret_cast<const lxb_char_t*>(name), strlen(name), &len);
      if (v) old_val = std::string(reinterpret_cast<const char*>(v), len);
    }

    lxb_dom_element_remove_attribute(el,
        reinterpret_cast<const lxb_char_t*>(name), strlen(name));

    if (has_obs) {
      rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
          std::string(name), old_val);
    }

    JS_FreeCString(ctx, name);
  }
  return JS_UNDEFINED;
}

static JSValue js_el_hasAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_FALSE;
  bool has = lxb_dom_element_has_attribute(el,
      reinterpret_cast<const lxb_char_t*>(name), strlen(name));
  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, has);
}

static JSValue js_el_appendChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent = unwrap_element(ctx, this_val);
  if (!parent || argc < 1) return JS_NULL;
  auto* child = unwrap_element(ctx, argv[0]);
  if (!child) return JS_NULL;

  lxb_dom_node_t* child_node = lxb_dom_interface_node(child);

  lxb_dom_node_insert_child(lxb_dom_interface_node(parent), child_node);

  lxb_dom_node_t* prev_sib = child_node->prev;
  lxb_dom_node_t* next_sib = child_node->next;

  auto* rctx = get_ctx(ctx);
  if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, lxb_dom_interface_node(parent),
        { child_node }, {}, prev_sib, next_sib);
  }

  return JS_DupValue(ctx, argv[0]);
}

static JSValue js_el_removeChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent = unwrap_element(ctx, this_val);
  if (!parent || argc < 1) return JS_NULL;
  auto* child = unwrap_element(ctx, argv[0]);
  if (!child) return JS_NULL;
  lxb_dom_node_t* cn = lxb_dom_interface_node(child);
  if (cn->parent == lxb_dom_interface_node(parent)) {
    lxb_dom_node_t* prev_sib = cn->prev;
    lxb_dom_node_t* next_sib = cn->next;
    lxb_dom_node_remove(cn);

    auto* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
      rctx->mutation_observers->notifyChildList(ctx, lxb_dom_interface_node(parent),
          {}, { cn }, prev_sib, next_sib);
    }
  }
  return JS_DupValue(ctx, argv[0]);
}

static JSValue js_el_insertBefore(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  auto* newNode = unwrap_element(ctx, argv[0]);
  if (!newNode) return JS_NULL;

  lxb_dom_node_t* new_node = lxb_dom_interface_node(newNode);
  lxb_dom_node_t* parent_node = nullptr;

  if (argc < 2 || JS_IsNull(argv[1])) {
    auto* parent = unwrap_element(ctx, this_val);
    if (parent) {
      parent_node = lxb_dom_interface_node(parent);
      lxb_dom_node_insert_child(parent_node, new_node);
    }
  } else {
    auto* refNode = unwrap_element(ctx, argv[1]);
    if (refNode) {
      lxb_dom_node_t* ref = lxb_dom_interface_node(refNode);
      parent_node = ref->parent;
      lxb_dom_node_insert_before(ref, new_node);
    }
  }

  if (parent_node) {
    auto* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
      rctx->mutation_observers->notifyChildList(ctx, parent_node,
          { new_node }, {}, new_node->prev, new_node->next);
    }
  }

  return JS_DupValue(ctx, argv[0]);
}

static JSValue js_el_remove(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* el = unwrap_element(ctx, this_val);
  if (el) {
    lxb_dom_node_t* node = lxb_dom_interface_node(el);
    lxb_dom_node_t* parent_node = node->parent;
    lxb_dom_node_t* prev_sib = node->prev;
    lxb_dom_node_t* next_sib = node->next;
    lxb_dom_node_remove(node);

    if (parent_node) {
      auto* rctx = get_ctx(ctx);
      if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
        rctx->mutation_observers->notifyChildList(ctx, parent_node,
            {}, { node }, prev_sib, next_sib);
      }
    }
  }
  return JS_UNDEFINED;
}

static JSValue js_el_cloneNode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  bool deep = argc >= 1 && JS_ToBool(ctx, argv[0]) > 0;
  lxb_dom_node_t* clone = lxb_dom_node_clone(lxb_dom_interface_node(el), deep);
  return make_element(ctx, clone);
}

static JSValue js_el_matches(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_FALSE;
  bool result = get_doc(ctx)->matchesSelector(el, sel);
  JS_FreeCString(ctx, sel);
  return JS_NewBool(ctx, result);
}

static JSValue js_el_querySelector(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  void* found = get_doc(ctx)->querySelectorFromEl(el, sel);
  JS_FreeCString(ctx, sel);
  return make_element(ctx, found);
}

static JSValue js_el_querySelectorAll(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAllFromEl(el, sel);
  JS_FreeCString(ctx, sel);
  return make_element_array(ctx, results);
}

static JSValue js_el_getElementsByClassName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* names = JS_ToCString(ctx, argv[0]);
  if (!names) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAllFromEl(el, classNames_to_selector(names));
  JS_FreeCString(ctx, names);
  return make_element_array(ctx, results);
}

static JSValue js_el_getElementsByTagName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAllFromEl(el, tag);
  JS_FreeCString(ctx, tag);
  return make_element_array(ctx, results);
}

// ── Document methods ──────────────────────────────────────────────────────────

static JSValue js_doc_getElementById(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* id = JS_ToCString(ctx, argv[0]);
  if (!id) return JS_NULL;
  void* el = get_doc(ctx)->getElementById(id);
  JS_FreeCString(ctx, id);
  return make_element(ctx, el);
}

static JSValue js_doc_querySelector(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  void* el = get_doc(ctx)->querySelector_el(sel);
  JS_FreeCString(ctx, sel);
  return make_element(ctx, el);
}

static JSValue js_doc_querySelectorAll(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAll_el(sel);
  JS_FreeCString(ctx, sel);
  return make_element_array(ctx, results);
}

static JSValue js_doc_getElementsByClassName(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* names = JS_ToCString(ctx, argv[0]);
  if (!names) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAll_el(classNames_to_selector(names));
  JS_FreeCString(ctx, names);
  return make_element_array(ctx, results);
}

static JSValue js_doc_getElementsByTagName(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAll_el(tag);
  JS_FreeCString(ctx, tag);
  return make_element_array(ctx, results);
}

static JSValue js_doc_createElement(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NULL;
  void* el = get_doc(ctx)->createElement(tag);
  JS_FreeCString(ctx, tag);
  return make_element(ctx, el);
}

static JSValue js_doc_createTextNode(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* text = JS_ToCString(ctx, argv[0]);
  if (!text) return JS_NULL;
  void* node = get_doc(ctx)->createTextNode(text);
  JS_FreeCString(ctx, text);
  if (!node) return JS_NULL;
  // Text nodes share the element class so they can be passed to appendChild
  JSValue obj = JS_NewObjectClass(ctx, js_element_class_id);
  JS_SetOpaque(obj, node);
  return obj;
}

// ── Document property getters ─────────────────────────────────────────────────

static JSValue js_doc_get_body(JSContext* ctx, JSValue) {
  return make_element(ctx, get_doc(ctx)->body());
}

static JSValue js_doc_get_head(JSContext* ctx, JSValue) {
  return make_element(ctx, get_doc(ctx)->head());
}

static JSValue js_doc_get_documentElement(JSContext* ctx, JSValue) {
  return make_element(ctx, get_doc(ctx)->documentElement());
}

// ── Helper: define an accessor property ──────────────────────────────────────

using GetterFn = JSValue(*)(JSContext*, JSValue);
using SetterFn = JSValue(*)(JSContext*, JSValue, JSValue);

static void define_prop(JSContext* ctx, JSValue obj, const char* name,
                        GetterFn getter, SetterFn setter = nullptr) {
  JSAtom atom = JS_NewAtom(ctx, name);
  JSValue get_fn = JS_NewCFunction2(ctx, (JSCFunction*)getter, name, 0, JS_CFUNC_getter, 0);
  JSValue set_fn = setter
      ? JS_NewCFunction2(ctx, (JSCFunction*)setter, name, 1, JS_CFUNC_setter, 0)
      : JS_UNDEFINED;
  JS_DefinePropertyGetSet(ctx, obj, atom, get_fn, set_fn, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, atom);
}

// ── Timer helpers ─────────────────────────────────────────────────────────────

static int64_t dom_now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static JSValue js_setTimeout(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_NewInt32(ctx, 0);

  int32_t delay_ms = 0;
  if (argc >= 2) JS_ToInt32(ctx, &delay_ms, argv[1]);
  if (delay_ms < 0) delay_ms = 0;

  uint32_t id = rctx->next_timer_id++;
  Timer* t = new Timer();
  t->id = id;
  t->repeat = false;
  t->interval_ms = delay_ms;
  t->fire_at_ms = dom_now_ms() + delay_ms;
  t->callback = new JSValue(JS_DupValue(ctx, argv[0]));
  t->cancelled = false;

  rctx->timer_map[id] = t;
  rctx->timer_heap.push(t);
  return JS_NewInt32(ctx, (int32_t)id);
}

static JSValue js_setInterval(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_NewInt32(ctx, 0);

  int32_t interval_ms = 0;
  if (argc >= 2) JS_ToInt32(ctx, &interval_ms, argv[1]);
  if (interval_ms < 1) interval_ms = 1;

  uint32_t id = rctx->next_timer_id++;
  Timer* t = new Timer();
  t->id = id;
  t->repeat = true;
  t->interval_ms = interval_ms;
  t->fire_at_ms = dom_now_ms() + interval_ms;
  t->callback = new JSValue(JS_DupValue(ctx, argv[0]));
  t->cancelled = false;

  rctx->timer_map[id] = t;
  rctx->timer_heap.push(t);
  return JS_NewInt32(ctx, (int32_t)id);
}

static JSValue js_clearTimer(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_UNDEFINED;

  uint32_t id = 0;
  JS_ToUint32(ctx, &id, argv[0]);
  auto it = rctx->timer_map.find(id);
  if (it != rctx->timer_map.end()) {
    it->second->cancelled = true;
    // Free the callback now to save memory; mark as null
    if (it->second->callback) {
      JSValue* cb = static_cast<JSValue*>(it->second->callback);
      JS_FreeValue(ctx, *cb);
      delete cb;
      it->second->callback = nullptr;
    }
    rctx->timer_map.erase(it);
  }
  return JS_UNDEFINED;
}

// ── Event + addEventListener + dispatchEvent ───────────────────────────────────

static JSClassID js_event_class_id = 0;
static JSClassDef js_event_class = { "Event", .finalizer = nullptr };

static JSValue js_Event_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type_str ? type_str : ""));
  if (type_str) JS_FreeCString(ctx, type_str);

  bool bubbles = false;
  bool cancelable = false;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue b = JS_GetPropertyStr(ctx, argv[1], "bubbles");
    if (JS_IsException(b)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { bubbles = JS_ToBool(ctx, b) > 0; JS_FreeValue(ctx, b); }

    JSValue c = JS_GetPropertyStr(ctx, argv[1], "cancelable");
    if (JS_IsException(c)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { cancelable = JS_ToBool(ctx, c) > 0; JS_FreeValue(ctx, c); }
  }

  JS_SetPropertyStr(ctx, obj, "defaultPrevented",  JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "bubbles",           JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable",        JS_NewBool(ctx, cancelable));
  return obj;
}

static JSValue js_CustomEvent_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type_str ? type_str : ""));
  if (type_str) JS_FreeCString(ctx, type_str);

  JSValue detail = JS_UNDEFINED;
  bool bubbles = false;
  bool cancelable = false;

  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue d = JS_GetPropertyStr(ctx, argv[1], "detail");
    if (JS_IsException(d)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { detail = d; }

    JSValue b = JS_GetPropertyStr(ctx, argv[1], "bubbles");
    if (JS_IsException(b)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { bubbles = JS_ToBool(ctx, b) > 0; JS_FreeValue(ctx, b); }

    JSValue c = JS_GetPropertyStr(ctx, argv[1], "cancelable");
    if (JS_IsException(c)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { cancelable = JS_ToBool(ctx, c) > 0; JS_FreeValue(ctx, c); }
  }

  JS_SetPropertyStr(ctx, obj, "detail",           detail);
  JS_SetPropertyStr(ctx, obj, "bubbles",          JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable",       JS_NewBool(ctx, cancelable));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented", JS_NewBool(ctx, false));
  return obj;
}

// ── Event prototype methods ───────────────────────────────────────────────────

static bool get_bool_prop(JSContext* ctx, JSValue obj, const char* name) {
  JSValue v = JS_GetPropertyStr(ctx, obj, name);
  bool b = JS_ToBool(ctx, v) > 0;
  JS_FreeValue(ctx, v);
  return b;
}

static JSValue js_event_preventDefault(JSContext* ctx, JSValue this_val, int, JSValue*) {
  if (get_bool_prop(ctx, this_val, "cancelable"))
    JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_NewBool(ctx, true));
  return JS_UNDEFINED;
}

static JSValue js_event_stopPropagation(JSContext* ctx, JSValue this_val, int, JSValue*) {
  JS_SetPropertyStr(ctx, this_val, "__propagationStopped", JS_NewBool(ctx, true));
  return JS_UNDEFINED;
}

static JSValue js_event_stopImmediatePropagation(JSContext* ctx, JSValue this_val, int, JSValue*) {
  JS_SetPropertyStr(ctx, this_val, "__propagationStopped", JS_NewBool(ctx, true));
  JS_SetPropertyStr(ctx, this_val, "__immediatePropagationStopped", JS_NewBool(ctx, true));
  return JS_UNDEFINED;
}

static JSValue js_el_addEventListener(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;

  EventListener listener;
  listener.node = lxb_dom_interface_node(el);
  listener.event_type = type_str;
  listener.callback = new JSValue(JS_DupValue(ctx, argv[1]));
  JS_FreeCString(ctx, type_str);

  rctx->listeners.push_back(std::move(listener));
  return JS_UNDEFINED;
}

static JSValue js_el_removeEventListener(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  void* node = lxb_dom_interface_node(el);
  for (auto it = rctx->listeners.begin(); it != rctx->listeners.end(); ) {
    if (it->node == node && it->event_type == event_type) {
      JSValue* stored_cb = static_cast<JSValue*>(it->callback);
      if (JS_VALUE_GET_TAG(*stored_cb) == JS_VALUE_GET_TAG(argv[1]) &&
          JS_VALUE_GET_PTR(*stored_cb) == JS_VALUE_GET_PTR(argv[1])) {
        JS_FreeValue(ctx, *stored_cb);
        delete stored_cb;
        it = rctx->listeners.erase(it);
        break; // remove first matching listener only (DOM spec)
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
  return JS_UNDEFINED;
}

static JSValue dispatch_event_on_target(JSContext* ctx, RuntimeContext* rctx, JSValue event_obj, lxb_dom_node_t* target_node) {
  JSValue type_val = JS_GetPropertyStr(ctx, event_obj, "type");
  const char* type_str = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);
  if (!type_str) return JS_TRUE;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  bool bubbles = get_bool_prop(ctx, event_obj, "bubbles");

  JS_SetPropertyStr(ctx, event_obj, "target", make_element(ctx, target_node));
  JS_SetPropertyStr(ctx, event_obj, "__propagationStopped", JS_NewBool(ctx, false));

  std::vector<lxb_dom_node_t*> chain;
  chain.push_back(target_node);
  if (bubbles) {
    for (lxb_dom_node_t* p = target_node->parent; p; p = p->parent) chain.push_back(p);
  }

  for (lxb_dom_node_t* level : chain) {
    if (get_bool_prop(ctx, event_obj, "__propagationStopped")) break;

    JSValue current_target = make_element(ctx, level);
    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_DupValue(ctx, current_target));
    JS_SetPropertyStr(ctx, event_obj, "__immediatePropagationStopped", JS_NewBool(ctx, false));

    // Snapshot the listeners to avoid invalidation during iteration
    std::vector<JSValue> cbs_to_fire;
    for (auto& listener : rctx->listeners) {
      if (listener.node == level && listener.event_type == event_type) {
        JSValue* cb = static_cast<JSValue*>(listener.callback);
        cbs_to_fire.push_back(JS_DupValue(ctx, *cb));
      }
    }

    for (size_t i = 0; i < cbs_to_fire.size(); i++) {
      if (get_bool_prop(ctx, event_obj, "__immediatePropagationStopped")) {
        JS_FreeValue(ctx, cbs_to_fire[i]);
        continue;
      }
      JSValue ret = JS_Call(ctx, cbs_to_fire[i], current_target, 1, &event_obj);
      JS_FreeValue(ctx, cbs_to_fire[i]);
      if (JS_IsException(ret)) {
        JS_FreeValue(ctx, ret);
        for (size_t j = i + 1; j < cbs_to_fire.size(); j++) JS_FreeValue(ctx, cbs_to_fire[j]);
        JS_FreeValue(ctx, current_target);

        JSValue ex = JS_GetException(ctx);
        std::string err = "Event callback error";
        JSValue msg_prop = JS_GetPropertyStr(ctx, ex, "message");
        if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
          const char* s = JS_ToCString(ctx, msg_prop);
          if (s) { err = s; JS_FreeCString(ctx, s); }
        }
        JS_FreeValue(ctx, msg_prop);
        JS_FreeValue(ctx, ex);
        JS_ThrowInternalError(ctx, "%s", err.c_str());
        return JS_EXCEPTION;
      }
      JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, current_target);
  }

  bool defaultPrevented = get_bool_prop(ctx, event_obj, "defaultPrevented");
  return JS_NewBool(ctx, !defaultPrevented);
}

static JSValue js_el_dispatchEvent(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_TRUE;
  return dispatch_event_on_target(ctx, rctx, argv[0], lxb_dom_interface_node(el));
}

// ── document.addEventListener / dispatchEvent ─────────────────────────────────

static JSValue js_doc_addEventListener(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  if (!rctx->document) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;

  void* doc_node = lxb_dom_interface_node(
      lxb_dom_interface_document(
          static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));

  EventListener listener;
  listener.node = doc_node;
  listener.event_type = type_str;
  listener.callback = new JSValue(JS_DupValue(ctx, argv[1]));
  JS_FreeCString(ctx, type_str);

  rctx->listeners.push_back(std::move(listener));
  return JS_UNDEFINED;
}

static JSValue js_doc_dispatchEvent(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  if (!rctx->document) return JS_TRUE;

  lxb_dom_node_t* node = lxb_dom_interface_node(
      lxb_dom_interface_document(
          static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));
  return dispatch_event_on_target(ctx, rctx, argv[0], node);
}

// ── Console bindings ──────────────────────────────────────────────────────────

static JSValue js_console_method(JSContext* ctx, JSValue, int argc, JSValue* argv, int magic) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->console_callback) return JS_UNDEFINED;

  static const char* levels[] = { "log", "warn", "error", "info", "debug" };
  std::string level = (magic >= 0 && magic < 5) ? levels[magic] : "log";

  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; i++) {
    JSValue str_val = JS_ToString(ctx, argv[i]);
    const char* s = JS_ToCString(ctx, str_val);
    args.push_back(s ? s : "");
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, str_val);
  }

  rctx->console_callback(level, args);
  return JS_UNDEFINED;
}

// ── window.alert / confirm / prompt ──────────────────────────────────────────

static JSValue js_window_alert(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->alert_callback) return JS_UNDEFINED;
  std::string message;
  if (argc >= 1) {
    JSValue str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { message = s; JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  rctx->alert_callback(message);
  return JS_UNDEFINED;
}

static JSValue js_window_confirm(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->confirm_callback) return JS_FALSE;
  std::string message;
  if (argc >= 1) {
    JSValue str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { message = s; JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  bool result = rctx->confirm_callback(message);
  return JS_NewBool(ctx, result);
}

static JSValue js_window_prompt(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->prompt_callback) return JS_NULL;
  std::string message;
  if (argc >= 1) {
    JSValue str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { message = s; JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  std::optional<std::string> defaultValue;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    JSValue str_val = JS_ToString(ctx, argv[1]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { defaultValue = std::string(s); JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  std::optional<std::string> result = rctx->prompt_callback(message, defaultValue);
  if (result.has_value()) {
    return JS_NewStringLen(ctx, result->c_str(), result->size());
  }
  return JS_NULL;
}

// ── fetch() bridge ──────────────────────────────────────────────────────────

static JSValue js_fetch_native(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->fetch_callback) {
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "fetch is not available: no onFetch handler configured"));
    return result;
  }

  auto toStr = [&](JSValue v) -> std::string {
    JSValue s = JS_ToString(ctx, v);
    std::string out;
    const char* c = JS_ToCString(ctx, s);
    if (c) { out = c; JS_FreeCString(ctx, c); }
    JS_FreeValue(ctx, s);
    return out;
  };

  std::string url    = argc >= 1 ? toStr(argv[0]) : "";
  std::string method = argc >= 2 ? toStr(argv[1]) : "GET";
  std::string headersJson = argc >= 3 ? toStr(argv[2]) : "{}";
  std::optional<std::string> body;
  if (argc >= 4 && !JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
    body = toStr(argv[3]);
  }

  std::string response = rctx->fetch_callback(url, method, headersJson, body);

  JSValue parsed = JS_ParseJSON(ctx, response.c_str(), response.size(), "<fetch-response>");
  if (JS_IsException(parsed)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "fetch: invalid response from native layer"));
    return result;
  }
  return parsed;
}

static const char* kFetchBootstrapScript = R"JS(
(function() {
  function normalizeHeaders(h) {
    var out = {};
    if (!h) return out;
    if (typeof h.entries === 'function') {
      for (var pair of h.entries()) out[pair[0]] = pair[1];
    } else {
      for (var k in h) out[k] = h[k];
    }
    return out;
  }

  function Headers(init) {
    this._map = {};
    var norm = normalizeHeaders(init);
    for (var k in norm) this._map[String(k).toLowerCase()] = String(norm[k]);
  }
  Headers.prototype.get = function(name) {
    var v = this._map[String(name).toLowerCase()];
    return v === undefined ? null : v;
  };
  Headers.prototype.has = function(name) {
    return Object.prototype.hasOwnProperty.call(this._map, String(name).toLowerCase());
  };
  Headers.prototype.set = function(name, value) {
    this._map[String(name).toLowerCase()] = String(value);
  };
  Headers.prototype.forEach = function(cb) {
    for (var k in this._map) cb(this._map[k], k, this);
  };
  Headers.prototype.entries = function() {
    var self = this;
    var keys = Object.keys(this._map);
    var i = 0;
    var iter = {
      next: function() {
        if (i >= keys.length) return { done: true, value: undefined };
        var k = keys[i++];
        return { done: false, value: [k, self._map[k]] };
      }
    };
    iter[Symbol.iterator] = function() { return iter; };
    return iter;
  };
  globalThis.Headers = Headers;

  function Response(bodyText, init) {
    init = init || {};
    this._bodyText = (bodyText === undefined || bodyText === null) ? '' : bodyText;
    this.status = init.status !== undefined ? init.status : 200;
    this.statusText = init.statusText !== undefined ? init.statusText : '';
    this.ok = this.status >= 200 && this.status < 300;
    this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    this.bodyUsed = false;
  }
  Response.prototype.text = function() {
    this.bodyUsed = true;
    return Promise.resolve(this._bodyText);
  };
  Response.prototype.json = function() {
    this.bodyUsed = true;
    return Promise.resolve(JSON.parse(this._bodyText));
  };
  Response.prototype.clone = function() {
    return new Response(this._bodyText, { status: this.status, statusText: this.statusText, headers: this.headers });
  };
  globalThis.Response = Response;

  globalThis.fetch = function(input, init) {
    return new Promise(function(resolve, reject) {
      try {
        var url = typeof input === 'string' ? input : (input && input.url);
        init = init || {};
        var method = (init.method || 'GET').toUpperCase();
        var headers = normalizeHeaders(init.headers);
        var headersJson = JSON.stringify(headers);
        var body = (init.body === undefined || init.body === null) ? undefined : String(init.body);

        var raw = __nativeFetchSync(url, method, headersJson, body);
        if (raw.error) {
          reject(new TypeError(String(raw.error)));
          return;
        }

        var respHeaders = {};
        try { respHeaders = JSON.parse(raw.headersJson || '{}'); } catch (e) {}

        resolve(new Response(raw.body, {
          status: raw.status,
          statusText: raw.statusText,
          headers: respHeaders,
        }));
      } catch (e) {
        reject(e);
      }
    });
  };

  // ── XMLHttpRequest ──────────────────────────────────────────────────────
  // send() is fully synchronous: __nativeFetchSync blocks the calling thread
  // until the response arrives, so all readyState transitions and events
  // fire within the same call frame, before send() returns.

  function XMLHttpRequest() {
    this.readyState = XMLHttpRequest.UNSENT;
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this.responseURL = '';
    this._method = null;
    this._url = null;
    this._requestHeaders = {};
    this._responseHeaders = {};
    this._listeners = {};
    this.onreadystatechange = null;
    this.onload = null;
    this.onerror = null;
    this.onabort = null;
    this.onloadend = null;
  }

  XMLHttpRequest.UNSENT = 0;
  XMLHttpRequest.OPENED = 1;
  XMLHttpRequest.HEADERS_RECEIVED = 2;
  XMLHttpRequest.LOADING = 3;
  XMLHttpRequest.DONE = 4;

  XMLHttpRequest.prototype.UNSENT = 0;
  XMLHttpRequest.prototype.OPENED = 1;
  XMLHttpRequest.prototype.HEADERS_RECEIVED = 2;
  XMLHttpRequest.prototype.LOADING = 3;
  XMLHttpRequest.prototype.DONE = 4;

  Object.defineProperty(XMLHttpRequest.prototype, 'response', {
    get: function() { return this.responseText; }
  });

  XMLHttpRequest.prototype._setReadyState = function(state) {
    this.readyState = state;
    this._dispatch('readystatechange');
  };

  XMLHttpRequest.prototype._dispatch = function(type) {
    var evt = new Event(type);
    evt.target = this;
    var handlerName = 'on' + type;
    if (typeof this[handlerName] === 'function') {
      try { this[handlerName](evt); } catch (e) {}
    }
    var listeners = this._listeners[type];
    if (listeners) {
      listeners.slice().forEach(function(cb) {
        try { cb(evt); } catch (e) {}
      });
    }
  };

  XMLHttpRequest.prototype.open = function(method, url) {
    this._method = String(method || 'GET').toUpperCase();
    this._url = String(url || '');
    this._requestHeaders = {};
    this._responseHeaders = {};
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this.responseURL = '';
    this._setReadyState(XMLHttpRequest.OPENED);
  };

  XMLHttpRequest.prototype.setRequestHeader = function(name, value) {
    if (this.readyState !== XMLHttpRequest.OPENED) {
      var err = new Error('InvalidStateError: setRequestHeader() requires open() to have been called first');
      err.name = 'InvalidStateError';
      throw err;
    }
    this._requestHeaders[String(name).toLowerCase()] = String(value);
  };

  XMLHttpRequest.prototype.send = function(body) {
    if (this.readyState !== XMLHttpRequest.OPENED) {
      var err = new Error('InvalidStateError: send() requires open() to have been called first');
      err.name = 'InvalidStateError';
      throw err;
    }

    var headersJson = JSON.stringify(this._requestHeaders);
    var bodyArg = (body === undefined || body === null) ? undefined : String(body);

    var raw = __nativeFetchSync(this._url, this._method, headersJson, bodyArg);

    if (raw.error) {
      this.status = 0;
      this.statusText = '';
      this.responseText = '';
      this.responseURL = '';
      this._responseHeaders = {};
      this._setReadyState(XMLHttpRequest.DONE);
      this._dispatch('error');
      this._dispatch('loadend');
      return;
    }

    var respHeaders = {};
    try { respHeaders = JSON.parse(raw.headersJson || '{}'); } catch (e) {}
    var normalized = {};
    for (var k in respHeaders) normalized[String(k).toLowerCase()] = String(respHeaders[k]);
    this._responseHeaders = normalized;
    this.status = raw.status;
    this.statusText = raw.statusText || '';
    this.responseURL = this._url;

    this._setReadyState(XMLHttpRequest.HEADERS_RECEIVED);
    this._setReadyState(XMLHttpRequest.LOADING);
    this.responseText = raw.body || '';
    this._setReadyState(XMLHttpRequest.DONE);
    this._dispatch('load');
    this._dispatch('loadend');
  };

  XMLHttpRequest.prototype.abort = function() {
    if (this.readyState === XMLHttpRequest.UNSENT || this.readyState === XMLHttpRequest.DONE) {
      return;
    }
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this._setReadyState(XMLHttpRequest.DONE);
    this._dispatch('abort');
    this._dispatch('loadend');
  };

  XMLHttpRequest.prototype.getAllResponseHeaders = function() {
    var lines = [];
    for (var k in this._responseHeaders) lines.push(k + ': ' + this._responseHeaders[k]);
    return lines.join('\r\n');
  };

  XMLHttpRequest.prototype.getResponseHeader = function(name) {
    var v = this._responseHeaders[String(name).toLowerCase()];
    return v === undefined ? null : v;
  };

  XMLHttpRequest.prototype.addEventListener = function(type, cb) {
    if (typeof cb !== 'function') return;
    if (!this._listeners[type]) this._listeners[type] = [];
    this._listeners[type].push(cb);
  };

  XMLHttpRequest.prototype.removeEventListener = function(type, cb) {
    var arr = this._listeners[type];
    if (!arr) return;
    var idx = arr.indexOf(cb);
    if (idx !== -1) arr.splice(idx, 1);
  };

  globalThis.XMLHttpRequest = XMLHttpRequest;
})();
)JS";

// ── window.location ─────────────────────────────────────────────────────────

static const char* kLocationBootstrapScript = R"JS(
(function() {
  function parseUrl(str) {
    str = String(str === undefined || str === null ? '' : str);
    var m = /^([^:\/?#]+:)(?:\/\/(?:[^\/?#@]*@)?([^\/?#:]*)(?::(\d+))?)?([^?#]*)(\?[^#]*)?(#.*)?$/.exec(str);
    if (!m) {
      return { protocol: '', hostname: '', port: '', pathname: '', search: '', hash: '' };
    }
    var protocol = m[1] || '';
    var hostname = m[2] || '';
    var port = m[3] || '';
    var pathname = m[4] || '';
    var search = m[5] || '';
    var hash = m[6] || '';
    if (!pathname && hostname) pathname = '/';
    return { protocol: protocol, hostname: hostname, port: port, pathname: pathname, search: search, hash: hash };
  }

  function Location(initialHref) {
    this._href = String(initialHref || 'about:blank');
  }

  function defineParsedProp(name) {
    Object.defineProperty(Location.prototype, name, {
      get: function() { return parseUrl(this._href)[name]; },
      enumerable: true,
      configurable: true,
    });
  }
  ['protocol', 'hostname', 'port', 'pathname', 'search', 'hash'].forEach(defineParsedProp);

  Object.defineProperty(Location.prototype, 'host', {
    get: function() {
      var p = parseUrl(this._href);
      return p.hostname + (p.port ? ':' + p.port : '');
    },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(Location.prototype, 'origin', {
    get: function() {
      var p = parseUrl(this._href);
      if (!p.hostname) return 'null';
      return p.protocol + '//' + p.hostname + (p.port ? ':' + p.port : '');
    },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(Location.prototype, 'href', {
    get: function() { return this._href; },
    set: function(v) { this._href = String(v); },
    enumerable: true,
    configurable: true,
  });

  Location.prototype.toString = function() { return this._href; };
  Location.prototype.assign = function(url) { this._href = String(url); };
  Location.prototype.replace = function(url) { this._href = String(url); };
  Location.prototype.reload = function() {};

  globalThis.Location = Location;
  globalThis.location = new Location(globalThis.__initialHref);
  delete globalThis.__initialHref;
})();
)JS";

// ── document.title ─────────────────────────────────────────────────────────

static const char* kDocumentTitleBootstrapScript = R"JS(
(function() {
  Object.defineProperty(document, 'title', {
    get: function() {
      var head = document.head;
      if (!head) return '';
      var titleEl = head.querySelector('title');
      return titleEl ? titleEl.textContent : '';
    },
    set: function(value) {
      var head = document.head;
      if (!head) return;
      var titleEl = head.querySelector('title');
      if (!titleEl) {
        titleEl = document.createElement('title');
        head.appendChild(titleEl);
      }
      titleEl.textContent = String(value);
    },
    enumerable: true,
    configurable: true,
  });
})();
)JS";

// ── DOMBindings::install ──────────────────────────────────────────────────────

void DOMBindings::install(QuickJSRuntime* runtime, LexborDocument* document) {
  JSContext* ctx = static_cast<JSContext*>(runtime->context());
  (void)document;

  // Register JS classes
  JS_NewClassID(&js_element_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);
  JS_NewClassID(&js_classList_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_classList_class_id, &js_classList_class);
  JS_NewClassID(&js_dataset_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_dataset_class_id, &js_dataset_class);
  JS_NewClassID(&js_style_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_style_class_id, &js_style_class);

  // ── style prototype (cssText accessor + setProperty/getPropertyValue/removeProperty) ──
  JSValue style_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, style_proto, "setProperty",      JS_NewCFunction(ctx, js_style_setProperty,      "setProperty",      2));
  JS_SetPropertyStr(ctx, style_proto, "getPropertyValue", JS_NewCFunction(ctx, js_style_getPropertyValue, "getPropertyValue", 1));
  JS_SetPropertyStr(ctx, style_proto, "removeProperty",   JS_NewCFunction(ctx, js_style_removeProperty,   "removeProperty",   1));
  define_prop(ctx, style_proto, "cssText", js_style_get_cssText, js_style_set_cssText);
  JS_SetClassProto(ctx, js_style_class_id, style_proto);
  JS_NewClassID(&js_event_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_event_class_id, &js_event_class);

  // ── Event prototype ────────────────────────────────────────────────────────
  JSValue event_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, event_proto, "preventDefault",            JS_NewCFunction(ctx, js_event_preventDefault,            "preventDefault",            0));
  JS_SetPropertyStr(ctx, event_proto, "stopPropagation",           JS_NewCFunction(ctx, js_event_stopPropagation,           "stopPropagation",           0));
  JS_SetPropertyStr(ctx, event_proto, "stopImmediatePropagation",  JS_NewCFunction(ctx, js_event_stopImmediatePropagation,  "stopImmediatePropagation",  0));
  JS_SetClassProto(ctx, js_event_class_id, event_proto);

  // ── Element prototype ──────────────────────────────────────────────────────
  JSValue proto = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, proto, "getAttribute",     JS_NewCFunction(ctx, js_el_getAttribute,     "getAttribute",     1));
  JS_SetPropertyStr(ctx, proto, "setAttribute",     JS_NewCFunction(ctx, js_el_setAttribute,     "setAttribute",     2));
  JS_SetPropertyStr(ctx, proto, "removeAttribute",  JS_NewCFunction(ctx, js_el_removeAttribute,  "removeAttribute",  1));
  JS_SetPropertyStr(ctx, proto, "hasAttribute",     JS_NewCFunction(ctx, js_el_hasAttribute,     "hasAttribute",     1));
  JS_SetPropertyStr(ctx, proto, "appendChild",      JS_NewCFunction(ctx, js_el_appendChild,      "appendChild",      1));
  JS_SetPropertyStr(ctx, proto, "removeChild",      JS_NewCFunction(ctx, js_el_removeChild,      "removeChild",      1));
  JS_SetPropertyStr(ctx, proto, "insertBefore",     JS_NewCFunction(ctx, js_el_insertBefore,     "insertBefore",     2));
  JS_SetPropertyStr(ctx, proto, "remove",           JS_NewCFunction(ctx, js_el_remove,           "remove",           0));
  JS_SetPropertyStr(ctx, proto, "cloneNode",        JS_NewCFunction(ctx, js_el_cloneNode,        "cloneNode",        1));
  JS_SetPropertyStr(ctx, proto, "matches",               JS_NewCFunction(ctx, js_el_matches,               "matches",               1));
  JS_SetPropertyStr(ctx, proto, "querySelector",         JS_NewCFunction(ctx, js_el_querySelector,         "querySelector",         1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll",      JS_NewCFunction(ctx, js_el_querySelectorAll,      "querySelectorAll",      1));
  JS_SetPropertyStr(ctx, proto, "getElementsByClassName", JS_NewCFunction(ctx, js_el_getElementsByClassName, "getElementsByClassName", 1));
  JS_SetPropertyStr(ctx, proto, "getElementsByTagName",   JS_NewCFunction(ctx, js_el_getElementsByTagName,   "getElementsByTagName",   1));
  JS_SetPropertyStr(ctx, proto, "addEventListener",      JS_NewCFunction(ctx, js_el_addEventListener,      "addEventListener",      2));
  JS_SetPropertyStr(ctx, proto, "removeEventListener",   JS_NewCFunction(ctx, js_el_removeEventListener,   "removeEventListener",   2));
  JS_SetPropertyStr(ctx, proto, "dispatchEvent",         JS_NewCFunction(ctx, js_el_dispatchEvent,         "dispatchEvent",         1));

  define_prop(ctx, proto, "tagName",                js_el_get_tagName,             nullptr);
  define_prop(ctx, proto, "id",                     js_el_get_id,                  js_el_set_id);
  define_prop(ctx, proto, "className",              js_el_get_className,           js_el_set_className);
  define_prop(ctx, proto, "textContent",            js_el_get_textContent,         js_el_set_textContent);
  define_prop(ctx, proto, "innerHTML",              js_el_get_innerHTML,           js_el_set_innerHTML);
  define_prop(ctx, proto, "outerHTML",              js_el_get_outerHTML,           nullptr);
  define_prop(ctx, proto, "parentElement",          js_el_get_parentElement,       nullptr);
  define_prop(ctx, proto, "children",               js_el_get_children,            nullptr);
  define_prop(ctx, proto, "firstElementChild",      js_el_get_firstElementChild,   nullptr);
  define_prop(ctx, proto, "lastElementChild",       js_el_get_lastElementChild,    nullptr);
  define_prop(ctx, proto, "nextElementSibling",     js_el_get_nextElementSibling,  nullptr);
  define_prop(ctx, proto, "previousElementSibling", js_el_get_prevElementSibling,  nullptr);
  define_prop(ctx, proto, "classList",              js_el_get_classList,           nullptr);
  define_prop(ctx, proto, "dataset",                js_el_get_dataset,             nullptr);
  define_prop(ctx, proto, "style",                  js_el_get_style,                nullptr);
  define_prop(ctx, proto, "childElementCount",      js_el_get_childElementCount,   nullptr);

  define_prop(ctx, proto, "nodeType",               js_el_get_nodeType,            nullptr);
  define_prop(ctx, proto, "nodeName",               js_el_get_nodeName,            nullptr);
  define_prop(ctx, proto, "nodeValue",              js_el_get_nodeValue,           js_el_set_nodeValue);
  define_prop(ctx, proto, "childNodes",             js_el_get_childNodes,          nullptr);
  define_prop(ctx, proto, "firstChild",             js_el_get_firstChild,          nullptr);
  define_prop(ctx, proto, "lastChild",              js_el_get_lastChild,           nullptr);
  define_prop(ctx, proto, "nextSibling",            js_el_get_nextSibling,         nullptr);
  define_prop(ctx, proto, "previousSibling",        js_el_get_previousSibling,     nullptr);
  define_prop(ctx, proto, "parentNode",             js_el_get_parentNode,          nullptr);

  JS_SetClassProto(ctx, js_element_class_id, proto);

  // ── document object ────────────────────────────────────────────────────────
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, doc, "getElementById",    JS_NewCFunction(ctx, js_doc_getElementById,    "getElementById",    1));
  JS_SetPropertyStr(ctx, doc, "querySelector",     JS_NewCFunction(ctx, js_doc_querySelector,     "querySelector",     1));
  JS_SetPropertyStr(ctx, doc, "querySelectorAll",  JS_NewCFunction(ctx, js_doc_querySelectorAll,  "querySelectorAll",  1));
  JS_SetPropertyStr(ctx, doc, "getElementsByClassName", JS_NewCFunction(ctx, js_doc_getElementsByClassName, "getElementsByClassName", 1));
  JS_SetPropertyStr(ctx, doc, "getElementsByTagName",   JS_NewCFunction(ctx, js_doc_getElementsByTagName,   "getElementsByTagName",   1));
  JS_SetPropertyStr(ctx, doc, "createElement",     JS_NewCFunction(ctx, js_doc_createElement,     "createElement",     1));
  JS_SetPropertyStr(ctx, doc, "createTextNode",      JS_NewCFunction(ctx, js_doc_createTextNode,     "createTextNode",      1));
  JS_SetPropertyStr(ctx, doc, "addEventListener",    JS_NewCFunction(ctx, js_doc_addEventListener,   "addEventListener",    2));
  JS_SetPropertyStr(ctx, doc, "dispatchEvent",       JS_NewCFunction(ctx, js_doc_dispatchEvent,      "dispatchEvent",       1));

  define_prop(ctx, doc, "body",            js_doc_get_body,            nullptr);
  define_prop(ctx, doc, "head",            js_doc_get_head,            nullptr);
  define_prop(ctx, doc, "documentElement", js_doc_get_documentElement, nullptr);

  JS_SetPropertyStr(ctx, global, "document", doc);

  // ── Global timer functions ─────────────────────────────────────────────────
  JS_SetPropertyStr(ctx, global, "setTimeout",   JS_NewCFunction(ctx, js_setTimeout,   "setTimeout",   2));
  JS_SetPropertyStr(ctx, global, "setInterval",  JS_NewCFunction(ctx, js_setInterval,  "setInterval",  2));
  JS_SetPropertyStr(ctx, global, "clearTimeout", JS_NewCFunction(ctx, js_clearTimer,   "clearTimeout", 1));
  JS_SetPropertyStr(ctx, global, "clearInterval",JS_NewCFunction(ctx, js_clearTimer,   "clearInterval",1));

  // ── Event constructor ──────────────────────────────────────────────────────
  JSValue event_ctor = JS_NewCFunction2(ctx, js_Event_constructor, "Event", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, global, "Event", event_ctor);

  // ── CustomEvent constructor ────────────────────────────────────────────────
  JSValue custom_event_ctor = JS_NewCFunction2(ctx, js_CustomEvent_constructor, "CustomEvent", 2, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, global, "CustomEvent", custom_event_ctor);

  // ── window.alert / confirm / prompt ───────────────────────────────────────
  JS_SetPropertyStr(ctx, global, "alert",   JS_NewCFunction(ctx, js_window_alert,   "alert",   1));
  JS_SetPropertyStr(ctx, global, "confirm", JS_NewCFunction(ctx, js_window_confirm, "confirm", 1));
  JS_SetPropertyStr(ctx, global, "prompt",  JS_NewCFunction(ctx, js_window_prompt,  "prompt",  2));

  // ── fetch() ────────────────────────────────────────────────────────────────
  JS_SetPropertyStr(ctx, global, "__nativeFetchSync", JS_NewCFunction(ctx, js_fetch_native, "__nativeFetchSync", 4));

  // ── localStorage / sessionStorage ──────────────────────────────────────────
  {
    RuntimeContext* rctx = get_ctx(ctx);
    if (rctx) {
      installStorage(ctx, "localStorage", &rctx->local_storage);
      installStorage(ctx, "sessionStorage", &rctx->session_storage);
    }
  }

  // ── MutationObserver ───────────────────────────────────────────────────────
  {
    RuntimeContext* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && rctx->document) {
      // Pass the lxb_dom_document_t* root node as the doc_root for ancestry checks
      void* html_doc = rctx->document->documentHtmlPtr();
      void* doc_node = lxb_dom_interface_node(
          lxb_dom_interface_document(
              static_cast<lxb_html_document_t*>(html_doc)));
      rctx->mutation_observers->install(ctx, doc_node);
    }
  }

  // ── console object ─────────────────────────────────────────────────────────
  JSValue console_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console_obj, "log",   JS_NewCFunctionMagic(ctx, js_console_method, "log",   0, JS_CFUNC_generic_magic, 0));
  JS_SetPropertyStr(ctx, console_obj, "warn",  JS_NewCFunctionMagic(ctx, js_console_method, "warn",  0, JS_CFUNC_generic_magic, 1));
  JS_SetPropertyStr(ctx, console_obj, "error", JS_NewCFunctionMagic(ctx, js_console_method, "error", 0, JS_CFUNC_generic_magic, 2));
  JS_SetPropertyStr(ctx, console_obj, "info",  JS_NewCFunctionMagic(ctx, js_console_method, "info",  0, JS_CFUNC_generic_magic, 3));
  JS_SetPropertyStr(ctx, console_obj, "debug", JS_NewCFunctionMagic(ctx, js_console_method, "debug", 0, JS_CFUNC_generic_magic, 4));
  JS_SetPropertyStr(ctx, global, "console", console_obj);

  JS_FreeValue(ctx, global);

  // ── JS-level fetch()/Headers/Response bootstrap ────────────────────────────
  JSValue bootstrap_result = JS_Eval(ctx, kFetchBootstrapScript, strlen(kFetchBootstrapScript),
                                      "<fetch-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(bootstrap_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, bootstrap_result);

  // ── JS-level window.location bootstrap ─────────────────────────────────────
  JSValue location_result = JS_Eval(ctx, kLocationBootstrapScript, strlen(kLocationBootstrapScript),
                                     "<location-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(location_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, location_result);

  // ── JS-level document.title bootstrap ──────────────────────────────────────
  JSValue title_result = JS_Eval(ctx, kDocumentTitleBootstrapScript, strlen(kDocumentTitleBootstrapScript),
                                  "<document-title-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(title_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, title_result);
}

} // namespace margelo::nitro::nitrojsdom
