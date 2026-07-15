#include "ElementBindings.hpp"
#include "ClassListBindings.hpp"
#include "DatasetBindings.hpp"
#include "StyleBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../MutationObservers.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <lexbor/dom/interfaces/character_data.h>
#include <cctype>
#include <cstring>
#include <optional>
#include <string>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassDef js_element_class = { "Element", .finalizer = nullptr };

// ── Element property getters/setters ─────────────────────────────────────────

JSValue js_el_get_tagName(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  size_t len = 0;
  const lxb_char_t* name = lxb_dom_element_local_name(el, &len);
  if (!name || len == 0) return JS_NewString(ctx, "");
  std::string r(reinterpret_cast<const char*>(name), len);
  for (auto& c : r) c = (char)std::toupper((unsigned char)c);
  return JS_NewStringLen(ctx, r.c_str(), r.size());
}

JSValue js_el_get_id(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  size_t len = 0;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>("id"), 2, &len);
  return val ? JS_NewStringLen(ctx, reinterpret_cast<const char*>(val), len) : JS_NewString(ctx, "");
}

JSValue js_el_set_id(JSContext* ctx, JSValue this_val, JSValue val) {
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

JSValue js_el_get_className(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  return JS_NewString(ctx, get_class_attr(el).c_str());
}

JSValue js_el_set_className(JSContext* ctx, JSValue this_val, JSValue val) {
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

JSValue js_el_get_textContent(JSContext* ctx, JSValue this_val) {
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

JSValue js_el_set_textContent(JSContext* ctx, JSValue this_val, JSValue val) {
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

JSValue js_el_get_innerHTML(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  std::string result;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { result += serialize_node(child); child = child->next; }
  return JS_NewString(ctx, result.c_str());
}

JSValue js_el_set_innerHTML(JSContext* ctx, JSValue this_val, JSValue val) {
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

JSValue js_el_get_outerHTML(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewString(ctx, "");
  return JS_NewString(ctx, serialize_node(lxb_dom_interface_node(el)).c_str());
}

JSValue js_el_get_parentElement(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* parent = lxb_dom_interface_node(el)->parent;
  if (!parent || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_element(parent));
}

JSValue js_el_get_children(JSContext* ctx, JSValue this_val) {
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

JSValue js_el_get_firstElementChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->first_child;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->next;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

JSValue js_el_get_lastElementChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->last_child;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->prev;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

JSValue js_el_get_nextElementSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->next;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->next;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

JSValue js_el_get_prevElementSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* n = lxb_dom_interface_node(el)->prev;
  while (n && n->type != LXB_DOM_NODE_TYPE_ELEMENT) n = n->prev;
  return n ? make_element(ctx, lxb_dom_interface_element(n)) : JS_NULL;
}

JSValue js_el_get_classList(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return ClassListBindings::make(ctx, el);
}

JSValue js_el_get_dataset(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return DatasetBindings::make(ctx, el);
}

JSValue js_el_get_style(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return StyleBindings::make(ctx, el);
}

JSValue js_el_get_childElementCount(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewInt32(ctx, 0);
  int count = 0;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { if (child->type == LXB_DOM_NODE_TYPE_ELEMENT) count++; child = child->next; }
  return JS_NewInt32(ctx, count);
}

// ── Generic Node traversal getters ────────────────────────────────────────────
// These operate on any lxb_dom_node_t-derived pointer (element, text, comment,
// document), since every such struct starts with an lxb_dom_node_t member.

JSValue js_el_get_nodeType(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewInt32(ctx, 0);
  return JS_NewInt32(ctx, (int32_t)lxb_dom_interface_node(el)->type);
}

JSValue js_el_get_nodeName(JSContext* ctx, JSValue this_val) {
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

JSValue js_el_get_nodeValue(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  if (node->type != LXB_DOM_NODE_TYPE_TEXT && node->type != LXB_DOM_NODE_TYPE_COMMENT) return JS_NULL;
  auto* cd = reinterpret_cast<lxb_dom_character_data_t*>(node);
  return JS_NewStringLen(ctx, reinterpret_cast<const char*>(cd->data.data), cd->data.length);
}

JSValue js_el_set_nodeValue(JSContext* ctx, JSValue this_val, JSValue val) {
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

JSValue js_el_get_childNodes(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  JSValue arr = JS_NewArray(ctx);
  if (!el) return arr;
  uint32_t idx = 0;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) { JS_SetPropertyUint32(ctx, arr, idx++, make_element(ctx, child)); child = child->next; }
  return arr;
}

JSValue js_el_get_firstChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->first_child);
}

JSValue js_el_get_lastChild(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->last_child);
}

JSValue js_el_get_nextSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->next);
}

JSValue js_el_get_previousSibling(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->prev);
}

JSValue js_el_get_parentNode(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_node(el)->parent);
}

// ── Element methods ───────────────────────────────────────────────────────────

JSValue js_el_getAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

JSValue js_el_setAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

JSValue js_el_removeAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

JSValue js_el_hasAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_FALSE;
  bool has = lxb_dom_element_has_attribute(el,
      reinterpret_cast<const lxb_char_t*>(name), strlen(name));
  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, has);
}

JSValue js_el_appendChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent = unwrap_element(ctx, this_val);
  if (!parent || argc < 1) return JS_NULL;
  auto* child = unwrap_element(ctx, argv[0]);
  if (!child) return JS_NULL;

  lxb_dom_node_t* child_node = lxb_dom_interface_node(child);

  lxb_dom_node_insert_child(lxb_dom_interface_node(parent), child_node);

  // Capture siblings AFTER insertion so they reflect the node's new position
  lxb_dom_node_t* prev_sib = child_node->prev;
  lxb_dom_node_t* next_sib = child_node->next;

  // Notify childList observers
  auto* rctx = get_ctx(ctx);
  if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, lxb_dom_interface_node(parent),
        { child_node }, {}, prev_sib, next_sib);
  }

  return JS_DupValue(ctx, argv[0]);
}

JSValue js_el_removeChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent = unwrap_element(ctx, this_val);
  if (!parent || argc < 1) return JS_NULL;
  auto* child = unwrap_element(ctx, argv[0]);
  if (!child) return JS_NULL;
  lxb_dom_node_t* cn = lxb_dom_interface_node(child);
  if (cn->parent == lxb_dom_interface_node(parent)) {
    lxb_dom_node_t* prev_sib = cn->prev;
    lxb_dom_node_t* next_sib = cn->next;
    lxb_dom_node_remove(cn);

    // Notify childList observers
    auto* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
      rctx->mutation_observers->notifyChildList(ctx, lxb_dom_interface_node(parent),
          {}, { cn }, prev_sib, next_sib);
    }
  }
  return JS_DupValue(ctx, argv[0]);
}

JSValue js_el_insertBefore(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
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

JSValue js_el_remove(JSContext* ctx, JSValue this_val, int, JSValue*) {
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

JSValue js_el_cloneNode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NULL;
  bool deep = argc >= 1 && JS_ToBool(ctx, argv[0]) > 0;
  lxb_dom_node_t* clone = lxb_dom_node_clone(lxb_dom_interface_node(el), deep);
  return make_element(ctx, clone);
}

JSValue js_el_matches(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_FALSE;
  bool result = get_doc(ctx)->matchesSelector(el, sel);
  JS_FreeCString(ctx, sel);
  return JS_NewBool(ctx, result);
}

JSValue js_el_querySelector(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  void* found = get_doc(ctx)->querySelectorFromEl(el, sel);
  JS_FreeCString(ctx, sel);
  return make_element(ctx, found);
}

JSValue js_el_querySelectorAll(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAllFromEl(el, sel);
  JS_FreeCString(ctx, sel);
  return make_element_array(ctx, results);
}

JSValue js_el_getElementsByClassName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* names = JS_ToCString(ctx, argv[0]);
  if (!names) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAllFromEl(el, classNames_to_selector(names));
  JS_FreeCString(ctx, names);
  return make_element_array(ctx, results);
}

JSValue js_el_getElementsByTagName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAllFromEl(el, tag);
  JS_FreeCString(ctx, tag);
  return make_element_array(ctx, results);
}

} // namespace

void ElementBindings::install(JSContext* ctx) {
  JS_NewClassID(&js_element_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);

  ClassListBindings::install(ctx);
  DatasetBindings::install(ctx);
  StyleBindings::install(ctx);

  JSValue proto = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, proto, "getAttribute",           JS_NewCFunction(ctx, js_el_getAttribute,           "getAttribute",           1));
  JS_SetPropertyStr(ctx, proto, "setAttribute",           JS_NewCFunction(ctx, js_el_setAttribute,           "setAttribute",           2));
  JS_SetPropertyStr(ctx, proto, "removeAttribute",        JS_NewCFunction(ctx, js_el_removeAttribute,        "removeAttribute",        1));
  JS_SetPropertyStr(ctx, proto, "hasAttribute",           JS_NewCFunction(ctx, js_el_hasAttribute,           "hasAttribute",           1));
  JS_SetPropertyStr(ctx, proto, "appendChild",            JS_NewCFunction(ctx, js_el_appendChild,            "appendChild",            1));
  JS_SetPropertyStr(ctx, proto, "removeChild",            JS_NewCFunction(ctx, js_el_removeChild,            "removeChild",            1));
  JS_SetPropertyStr(ctx, proto, "insertBefore",           JS_NewCFunction(ctx, js_el_insertBefore,           "insertBefore",           2));
  JS_SetPropertyStr(ctx, proto, "remove",                 JS_NewCFunction(ctx, js_el_remove,                 "remove",                 0));
  JS_SetPropertyStr(ctx, proto, "cloneNode",              JS_NewCFunction(ctx, js_el_cloneNode,              "cloneNode",              1));
  JS_SetPropertyStr(ctx, proto, "matches",                JS_NewCFunction(ctx, js_el_matches,                "matches",                1));
  JS_SetPropertyStr(ctx, proto, "querySelector",          JS_NewCFunction(ctx, js_el_querySelector,          "querySelector",          1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll",       JS_NewCFunction(ctx, js_el_querySelectorAll,       "querySelectorAll",       1));
  JS_SetPropertyStr(ctx, proto, "getElementsByClassName", JS_NewCFunction(ctx, js_el_getElementsByClassName, "getElementsByClassName", 1));
  JS_SetPropertyStr(ctx, proto, "getElementsByTagName",   JS_NewCFunction(ctx, js_el_getElementsByTagName,   "getElementsByTagName",   1));

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
  define_prop(ctx, proto, "style",                  js_el_get_style,               nullptr);
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
}

} // namespace margelo::nitro::nitrojsdom
