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
#include <functional>
#include <chrono>

namespace margelo::nitro::nitrojsdom {

// ── Class IDs ────────────────────────────────────────────────────────────────

static JSClassID js_element_class_id   = 0;
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
    // The Timer object itself will be cleaned up when the heap pops it
  }
  return JS_UNDEFINED;
}

// ── Event + addEventListener + dispatchEvent ───────────────────────────────────

static JSClassID js_event_class_id = 0;
static JSClassDef js_event_class = { "Event", .finalizer = nullptr };

// new Event('type') constructor
static JSValue js_Event_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type",             JS_NewString(ctx, type_str ? type_str : ""));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented",  JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "bubbles",           JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "cancelable",        JS_NewBool(ctx, false));
  if (type_str) JS_FreeCString(ctx, type_str);
  return obj;
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
      // Check JS function identity via pointer
      JSValue* stored_cb = static_cast<JSValue*>(it->callback);
      if (JS_VALUE_GET_PTR(*stored_cb) == JS_VALUE_GET_PTR(argv[1])) {
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

static JSValue js_el_dispatchEvent(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_TRUE;

  void* node = lxb_dom_interface_node(el);
  JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
  const char* type_str = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);
  if (!type_str) return JS_TRUE;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  // Snapshot the listeners to avoid invalidation during iteration
  std::vector<JSValue> cbs_to_fire;
  for (auto& listener : rctx->listeners) {
    if (listener.node == node && listener.event_type == event_type) {
      JSValue* cb = static_cast<JSValue*>(listener.callback);
      cbs_to_fire.push_back(JS_DupValue(ctx, *cb));
    }
  }

  for (auto& cb : cbs_to_fire) {
    JSValue ret = JS_Call(ctx, cb, this_val, 1, argv);
    JS_FreeValue(ctx, cb);
    if (JS_IsException(ret)) {
      JS_FreeValue(ctx, ret);
      // Free remaining dup'd callbacks
      for (size_t i = (&cb - cbs_to_fire.data()) + 1; i < cbs_to_fire.size(); i++) {
        JS_FreeValue(ctx, cbs_to_fire[i]);
      }
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
  return JS_TRUE;
}

// ── document.addEventListener / dispatchEvent ─────────────────────────────────

static JSValue js_doc_addEventListener(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  auto* doc = rctx->document ? rctx->document->documentElement() : nullptr;
  if (!doc) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;

  EventListener listener;
  listener.node = lxb_dom_interface_node(static_cast<lxb_dom_element_t*>(doc));
  listener.event_type = type_str;
  listener.callback = new JSValue(JS_DupValue(ctx, argv[1]));
  JS_FreeCString(ctx, type_str);

  rctx->listeners.push_back(std::move(listener));
  return JS_UNDEFINED;
}

static JSValue js_doc_dispatchEvent(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  auto* doc_el = rctx->document ? rctx->document->documentElement() : nullptr;
  if (!doc_el) return JS_TRUE;

  void* node = lxb_dom_interface_node(static_cast<lxb_dom_element_t*>(doc_el));
  JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
  const char* type_str = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);
  if (!type_str) return JS_TRUE;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  std::vector<JSValue> cbs_to_fire;
  for (auto& listener : rctx->listeners) {
    if (listener.node == node && listener.event_type == event_type) {
      JSValue* cb = static_cast<JSValue*>(listener.callback);
      cbs_to_fire.push_back(JS_DupValue(ctx, *cb));
    }
  }

  for (auto& cb : cbs_to_fire) {
    JSValue ret = JS_Call(ctx, cb, JS_UNDEFINED, 1, argv);
    JS_FreeValue(ctx, cb);
    if (JS_IsException(ret)) {
      JS_FreeValue(ctx, ret);
      return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, ret);
  }
  return JS_TRUE;
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

// ── DOMBindings::install ──────────────────────────────────────────────────────

void DOMBindings::install(QuickJSRuntime* runtime, LexborDocument* document) {
  JSContext* ctx = static_cast<JSContext*>(runtime->context());
  // Note: JS_SetContextOpaque is now set in QuickJSRuntime::initialize to RuntimeContext*.
  // document is already set on RuntimeContext by QuickJSRuntime::bindDocument before calling here.
  (void)document; // RuntimeContext already holds the document pointer

  // Register JS classes
  JS_NewClassID(&js_element_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);
  JS_NewClassID(&js_classList_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_classList_class_id, &js_classList_class);
  JS_NewClassID(&js_event_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_event_class_id, &js_event_class);

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
  JS_SetPropertyStr(ctx, proto, "matches",               JS_NewCFunction(ctx, js_el_matches,               "matches",               1));
  JS_SetPropertyStr(ctx, proto, "querySelector",         JS_NewCFunction(ctx, js_el_querySelector,         "querySelector",         1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll",      JS_NewCFunction(ctx, js_el_querySelectorAll,      "querySelectorAll",      1));
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
  define_prop(ctx, proto, "childElementCount",      js_el_get_childElementCount,   nullptr);

  JS_SetClassProto(ctx, js_element_class_id, proto);

  // ── document object ────────────────────────────────────────────────────────
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, doc, "getElementById",    JS_NewCFunction(ctx, js_doc_getElementById,    "getElementById",    1));
  JS_SetPropertyStr(ctx, doc, "querySelector",     JS_NewCFunction(ctx, js_doc_querySelector,     "querySelector",     1));
  JS_SetPropertyStr(ctx, doc, "querySelectorAll",  JS_NewCFunction(ctx, js_doc_querySelectorAll,  "querySelectorAll",  1));
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

  // ── console object ─────────────────────────────────────────────────────────
  JSValue console_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console_obj, "log",   JS_NewCFunctionMagic(ctx, js_console_method, "log",   0, JS_CFUNC_generic_magic, 0));
  JS_SetPropertyStr(ctx, console_obj, "warn",  JS_NewCFunctionMagic(ctx, js_console_method, "warn",  0, JS_CFUNC_generic_magic, 1));
  JS_SetPropertyStr(ctx, console_obj, "error", JS_NewCFunctionMagic(ctx, js_console_method, "error", 0, JS_CFUNC_generic_magic, 2));
  JS_SetPropertyStr(ctx, console_obj, "info",  JS_NewCFunctionMagic(ctx, js_console_method, "info",  0, JS_CFUNC_generic_magic, 3));
  JS_SetPropertyStr(ctx, console_obj, "debug", JS_NewCFunctionMagic(ctx, js_console_method, "debug", 0, JS_CFUNC_generic_magic, 4));
  JS_SetPropertyStr(ctx, global, "console", console_obj);

  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
