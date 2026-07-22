#include "ClassListBindings.hpp"
#include "DOMExceptionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../MutationObservers.hpp"
#include <lexbor/dom/dom.h>
#include <algorithm>
#include <optional>
#include <sstream>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_classList_class_id = 0;
JSClassDef js_classList_class = { "DOMTokenList", .finalizer = nullptr };

lxb_dom_element_t* unwrap_classList(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_classList_class_id));
}

std::vector<std::string> split_classes(const std::string& str) {
  std::vector<std::string> result;
  std::istringstream iss(str);
  std::string token;
  while (iss >> token) result.push_back(token);
  return result;
}

std::string join_classes(const std::vector<std::string>& v) {
  std::string r;
  for (size_t i = 0; i < v.size(); i++) { if (i) r += ' '; r += v[i]; }
  return r;
}

// Validates a DOMTokenList token: throws on empty or whitespace-containing token.
bool validate_token(JSContext* ctx, const char* token) {
  if (!token || token[0] == '\0') {
    throw_dom_exception(ctx, "SyntaxError", "The token provided must not be empty.");
    return false;
  }
  for (const char* p = token; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
      throw_dom_exception(ctx, "InvalidCharacterError", "The token provided contains HTML space characters, which are not valid in tokens.");
      return false;
    }
  }
  return true;
}

JSValue js_classList_add(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  // Validate all tokens before mutating (spec: validate each, then add all)
  std::vector<std::string> tokens;
  for (int i = 0; i < argc; i++) {
    const char* cls = JS_ToCString(ctx, argv[i]);
    if (!cls) return JS_EXCEPTION;
    if (!validate_token(ctx, cls)) { JS_FreeCString(ctx, cls); return JS_EXCEPTION; }
    tokens.emplace_back(cls);
    JS_FreeCString(ctx, cls);
  }

  auto classes = split_classes(get_class_attr(el));
  for (auto& tok : tokens) {
    if (std::find(classes.begin(), classes.end(), tok) == classes.end())
      classes.push_back(tok);
  }
  set_class_attr(el, join_classes(classes));

  if (has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
        "class", old_val);
  }
  return JS_UNDEFINED;
}

JSValue js_classList_remove(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  // Validate all tokens before mutating
  std::vector<std::string> tokens;
  for (int i = 0; i < argc; i++) {
    const char* cls = JS_ToCString(ctx, argv[i]);
    if (!cls) return JS_EXCEPTION;
    if (!validate_token(ctx, cls)) { JS_FreeCString(ctx, cls); return JS_EXCEPTION; }
    tokens.emplace_back(cls);
    JS_FreeCString(ctx, cls);
  }

  auto classes = split_classes(get_class_attr(el));
  for (auto& tok : tokens)
    classes.erase(std::remove(classes.begin(), classes.end(), tok), classes.end());
  set_class_attr(el, join_classes(classes));

  if (has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
        "class", old_val);
  }
  return JS_UNDEFINED;
}

JSValue js_classList_contains(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* cls = JS_ToCString(ctx, argv[0]);
  if (!cls) return JS_EXCEPTION;
  if (!validate_token(ctx, cls)) { JS_FreeCString(ctx, cls); return JS_EXCEPTION; }
  auto classes = split_classes(get_class_attr(el));
  bool found = std::find(classes.begin(), classes.end(), cls) != classes.end();
  JS_FreeCString(ctx, cls);
  return JS_NewBool(ctx, found);
}

JSValue js_classList_toggle(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* cls = JS_ToCString(ctx, argv[0]);
  if (!cls) return JS_EXCEPTION;
  if (!validate_token(ctx, cls)) { JS_FreeCString(ctx, cls); return JS_EXCEPTION; }

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  auto classes = split_classes(get_class_attr(el));
  auto it = std::find(classes.begin(), classes.end(), cls);

  bool added;
  bool force_present = argc >= 2 && !JS_IsUndefined(argv[1]);
  if (force_present) {
    bool force = JS_ToBool(ctx, argv[1]) > 0;
    if (force) {
      if (it == classes.end()) classes.push_back(cls);
      added = true;
    } else {
      if (it != classes.end()) classes.erase(it);
      added = false;
    }
  } else {
    if (it != classes.end()) { classes.erase(it); added = false; }
    else { classes.push_back(cls); added = true; }
  }

  JS_FreeCString(ctx, cls);
  set_class_attr(el, join_classes(classes));

  if (has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
        "class", old_val);
  }
  return JS_NewBool(ctx, added);
}

JSValue js_classList_replace(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 2) return JS_FALSE;
  const char* oldCls = JS_ToCString(ctx, argv[0]);
  if (!oldCls) return JS_EXCEPTION;
  if (!validate_token(ctx, oldCls)) { JS_FreeCString(ctx, oldCls); return JS_EXCEPTION; }
  const char* newCls = JS_ToCString(ctx, argv[1]);
  if (!newCls) { JS_FreeCString(ctx, oldCls); return JS_EXCEPTION; }
  if (!validate_token(ctx, newCls)) { JS_FreeCString(ctx, oldCls); JS_FreeCString(ctx, newCls); return JS_EXCEPTION; }

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && rctx->mutation_observers->hasAttributeOldValueObserver())
    old_val = get_class_attr(el);

  auto classes = split_classes(get_class_attr(el));
  auto it = std::find(classes.begin(), classes.end(), oldCls);
  bool replaced = false;
  if (it != classes.end()) {
    *it = newCls;
    // Remove any other occurrences of newCls to keep the set unique
    classes.erase(std::remove(it + 1, classes.end(), newCls), classes.end());
    replaced = true;
    set_class_attr(el, join_classes(classes));
    if (has_obs) {
      rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el),
          "class", old_val);
    }
  }

  JS_FreeCString(ctx, oldCls);
  JS_FreeCString(ctx, newCls);
  return JS_NewBool(ctx, replaced);
}

} // namespace

std::string get_class_attr(lxb_dom_element_t* el) {
  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>("class"), 5, &len);
  return val ? std::string(reinterpret_cast<const char*>(val), len) : "";
}

void set_class_attr(lxb_dom_element_t* el, const std::string& classes) {
  // Always write the attribute (even when empty) so classList.value serializes
  // back to "" rather than the attribute disappearing after the last token is removed.
  lxb_dom_element_set_attribute(el,
      reinterpret_cast<const lxb_char_t*>("class"), 5,
      reinterpret_cast<const lxb_char_t*>(classes.data()), classes.size());
}

void ClassListBindings::install(JSContext* ctx) {
  if (js_classList_class_id == 0) JS_NewClassID(&js_classList_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_classList_class_id, &js_classList_class);
}

JSValue ClassListBindings::make(JSContext* ctx, lxb_dom_element_t* el) {
  JSValue obj = JS_NewObjectClass(ctx, js_classList_class_id);
  JS_SetOpaque(obj, el);
  JS_SetPropertyStr(ctx, obj, "add",      JS_NewCFunction(ctx, js_classList_add,      "add",      1));
  JS_SetPropertyStr(ctx, obj, "remove",   JS_NewCFunction(ctx, js_classList_remove,   "remove",   1));
  JS_SetPropertyStr(ctx, obj, "contains", JS_NewCFunction(ctx, js_classList_contains, "contains", 1));
  JS_SetPropertyStr(ctx, obj, "toggle",   JS_NewCFunction(ctx, js_classList_toggle,   "toggle",   1));
  JS_SetPropertyStr(ctx, obj, "replace",  JS_NewCFunction(ctx, js_classList_replace,  "replace",  2));
  return obj;
}

} // namespace margelo::nitro::nitrojsdom
