#include "DOMBindings.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "QuickJSRuntime.hpp"
#include "quickjs.h"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace margelo::nitro::nitrojsdom {

// ── Class IDs ────────────────────────────────────────────────────────────────

static JSClassID js_element_class_id   = 0;
static JSClassID js_classList_class_id = 0;

static JSClassDef js_element_class   = { "Element",      .finalizer = nullptr };
static JSClassDef js_classList_class = { "DOMTokenList", .finalizer = nullptr };

// ── Core helpers ──────────────────────────────────────────────────────────────

static LexborDocument* get_doc(JSContext* ctx) {
  return static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
}

static lxb_dom_element_t* unwrap_element(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_element_class_id));
}

static JSValue make_element(JSContext* ctx, void* el) {
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

// ── classList methods ─────────────────────────────────────────────────────────

static lxb_dom_element_t* unwrap_classList(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_classList_class_id));
}

static JSValue js_classList_add(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  auto classes = split_classes(get_class_attr(el));
  for (int i = 0; i < argc; i++) {
    const char* cls = JS_ToCString(ctx, argv[i]);
    if (cls) {
      if (std::find(classes.begin(), classes.end(), cls) == classes.end()) classes.push_back(cls);
      JS_FreeCString(ctx, cls);
    }
  }
  set_class_attr(el, join_classes(classes));
  return JS_UNDEFINED;
}

static JSValue js_classList_remove(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el) return JS_UNDEFINED;
  auto classes = split_classes(get_class_attr(el));
  for (int i = 0; i < argc; i++) {
    const char* cls = JS_ToCString(ctx, argv[i]);
    if (cls) { classes.erase(std::remove(classes.begin(), classes.end(), cls), classes.end()); JS_FreeCString(ctx, cls); }
  }
  set_class_attr(el, join_classes(classes));
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
  auto classes = split_classes(get_class_attr(el));
  auto it = std::find(classes.begin(), classes.end(), cls);
  bool added;
  if (it != classes.end()) { classes.erase(it); added = false; }
  else { classes.push_back(cls); added = true; }
  JS_FreeCString(ctx, cls);
  set_class_attr(el, join_classes(classes));
  return JS_NewBool(ctx, added);
}

static JSValue js_classList_replace(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_classList(ctx, this_val);
  if (!el || argc < 2) return JS_FALSE;
  const char* oldCls = JS_ToCString(ctx, argv[0]);
  const char* newCls = JS_ToCString(ctx, argv[1]);
  bool replaced = false;
  if (oldCls && newCls) {
    auto classes = split_classes(get_class_attr(el));
    auto it = std::find(classes.begin(), classes.end(), oldCls);
    if (it != classes.end()) { *it = newCls; replaced = true; set_class_attr(el, join_classes(classes)); }
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
    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>("id"), 2,
        reinterpret_cast<const lxb_char_t*>(str), strlen(str));
    JS_FreeCString(ctx, str);
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
  if (str) { set_class_attr(el, str); JS_FreeCString(ctx, str); }
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
  if (str) { get_doc(ctx)->setTextContentOnEl(el, str); JS_FreeCString(ctx, str); }
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
  if (html) { get_doc(ctx)->setInnerHTMLOnEl(el, html); JS_FreeCString(ctx, html); }
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

static JSValue js_el_get_childElementCount(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewInt32(ctx, 0);
  int count = 0;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) count++; child = child->next; }
  return JS_NewInt32(ctx, count);
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
  if (name && value)
    lxb_dom_element_set_attribute(el,
        reinterpret_cast<const lxb_char_t*>(name),  strlen(name),
        reinterpret_cast<const lxb_char_t*>(value), strlen(value));
  if (name)  JS_FreeCString(ctx, name);
  if (value) JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

static JSValue js_el_removeAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_UNDEFINED;
  const char* name = JS_ToCString(ctx, argv[0]);
  if (name) {
    lxb_dom_element_remove_attribute(el,
        reinterpret_cast<const lxb_char_t*>(name), strlen(name));
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
  lxb_dom_node_insert_child(lxb_dom_interface_node(parent), lxb_dom_interface_node(child));
  return JS_DupValue(ctx, argv[0]);
}

static JSValue js_el_removeChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent = unwrap_element(ctx, this_val);
  if (!parent || argc < 1) return JS_NULL;
  auto* child = unwrap_element(ctx, argv[0]);
  if (!child) return JS_NULL;
  lxb_dom_node_t* cn = lxb_dom_interface_node(child);
  if (cn->parent == lxb_dom_interface_node(parent)) lxb_dom_node_remove(cn);
  return JS_DupValue(ctx, argv[0]);
}

static JSValue js_el_insertBefore(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  auto* newNode = unwrap_element(ctx, argv[0]);
  if (!newNode) return JS_NULL;
  if (argc < 2 || JS_IsNull(argv[1])) {
    auto* parent = unwrap_element(ctx, this_val);
    if (parent) lxb_dom_node_insert_child(lxb_dom_interface_node(parent), lxb_dom_interface_node(newNode));
  } else {
    auto* refNode = unwrap_element(ctx, argv[1]);
    if (refNode) lxb_dom_node_insert_before(lxb_dom_interface_node(refNode), lxb_dom_interface_node(newNode));
  }
  return JS_DupValue(ctx, argv[0]);
}

static JSValue js_el_remove(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* el = unwrap_element(ctx, this_val);
  if (el) lxb_dom_node_remove(lxb_dom_interface_node(el));
  return JS_UNDEFINED;
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

// ── DOMBindings::install ──────────────────────────────────────────────────────

void DOMBindings::install(QuickJSRuntime* runtime, LexborDocument* document) {
  JSContext* ctx = static_cast<JSContext*>(runtime->context());
  JS_SetContextOpaque(ctx, document);

  // Register JS classes
  JS_NewClassID(&js_element_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);
  JS_NewClassID(&js_classList_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_classList_class_id, &js_classList_class);

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
  JS_SetPropertyStr(ctx, proto, "matches",          JS_NewCFunction(ctx, js_el_matches,          "matches",          1));
  JS_SetPropertyStr(ctx, proto, "querySelector",    JS_NewCFunction(ctx, js_el_querySelector,    "querySelector",    1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll", JS_NewCFunction(ctx, js_el_querySelectorAll, "querySelectorAll", 1));

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
  define_prop(ctx, proto, "childElementCount",      js_el_get_childElementCount,   nullptr);

  JS_SetClassProto(ctx, js_element_class_id, proto);

  // ── document object ────────────────────────────────────────────────────────
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, doc, "getElementById",    JS_NewCFunction(ctx, js_doc_getElementById,    "getElementById",    1));
  JS_SetPropertyStr(ctx, doc, "querySelector",     JS_NewCFunction(ctx, js_doc_querySelector,     "querySelector",     1));
  JS_SetPropertyStr(ctx, doc, "querySelectorAll",  JS_NewCFunction(ctx, js_doc_querySelectorAll,  "querySelectorAll",  1));
  JS_SetPropertyStr(ctx, doc, "createElement",     JS_NewCFunction(ctx, js_doc_createElement,     "createElement",     1));
  JS_SetPropertyStr(ctx, doc, "createTextNode",    JS_NewCFunction(ctx, js_doc_createTextNode,    "createTextNode",    1));

  define_prop(ctx, doc, "body",            js_doc_get_body,            nullptr);
  define_prop(ctx, doc, "head",            js_doc_get_head,            nullptr);
  define_prop(ctx, doc, "documentElement", js_doc_get_documentElement, nullptr);

  JS_SetPropertyStr(ctx, global, "document", doc);
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
