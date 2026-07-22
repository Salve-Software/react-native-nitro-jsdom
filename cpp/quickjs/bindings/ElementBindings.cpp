#include "ElementBindings.hpp"
#include "ClassListBindings.hpp"
#include "DatasetBindings.hpp"
#include "StyleBindings.hpp"
#include "LiveCollectionBindings.hpp"
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
JSClassDef js_node_class    = { "Node",    .finalizer = nullptr };

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
  lxb_dom_node_t* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NewString(ctx, "");
  size_t len = 0;
  lxb_char_t* text = lxb_dom_node_text_content(node, &len);
  if (!text) return JS_NewString(ctx, "");
  JSValue result = JS_NewStringLen(ctx, reinterpret_cast<char*>(text), len);
  lxb_dom_document_destroy_text(node->owner_document, text);
  return result;
}

JSValue js_el_set_textContent(JSContext* ctx, JSValue this_val, JSValue val) {
  lxb_dom_node_t* node = unwrap_node(ctx, this_val);
  if (!node) return JS_UNDEFINED;
  const char* str = JS_ToCString(ctx, val);
  if (!str) return JS_UNDEFINED;

  auto* rctx = get_ctx(ctx);
  bool has_observers = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();

  if (node->type == LXB_DOM_NODE_TYPE_TEXT ||
      node->type == LXB_DOM_NODE_TYPE_COMMENT) {
    std::optional<std::string> old_val;
    if (has_observers && rctx->mutation_observers->hasCharacterDataOldValueObserver()) {
      auto* cd = reinterpret_cast<lxb_dom_character_data_t*>(node);
      old_val = std::string(reinterpret_cast<const char*>(cd->data.data), cd->data.length);
    }
    auto* cd = reinterpret_cast<lxb_dom_character_data_t*>(node);
    lxb_dom_character_data_replace(cd, reinterpret_cast<const lxb_char_t*>(str), strlen(str), 0, cd->data.length);
    if (has_observers) {
      rctx->mutation_observers->notifyCharacterData(ctx, node, old_val);
    }
  } else {
    auto* el = reinterpret_cast<lxb_dom_element_t*>(node);
    // Element node: textContent replaces all children — fire childList.
    // Collect old children BEFORE destruction to disconnect their observers,
    // but do NOT forward the raw pointers to notifyChildList: setTextContentOnEl
    // frees those nodes, so passing them into the async dispatch job would be
    // a use-after-free. We report removed={} (nodes already gone by dispatch time).
    {
      std::vector<void*> old_children;
      lxb_dom_node_t* child = node->first_child;
      while (child) { old_children.push_back(child); child = child->next; }
      if (!old_children.empty()) {
        invalidate_node_cache_batch(ctx, rctx, old_children);
        if (has_observers) rctx->mutation_observers->disconnectDetachedTargets(old_children);
      }
    }
    get_doc(ctx)->setTextContentOnEl(el, str);
    if (has_observers) {
      std::vector<void*> new_children;
      lxb_dom_node_t* child = node->first_child;
      while (child) { new_children.push_back(child); child = child->next; }
      rctx->mutation_observers->notifyChildList(ctx, node,
          new_children, {}, nullptr, nullptr);
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

  std::vector<void*> old_children;
  {
    lxb_dom_node_t* child = parent->first_child;
    while (child) { old_children.push_back(child); child = child->next; }
  }

  if (!old_children.empty()) {
    invalidate_node_cache_batch(ctx, rctx, old_children);
    if (has_observers) rctx->mutation_observers->disconnectDetachedTargets(old_children);
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
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  lxb_dom_node_t* parent = node->parent;
  if (!parent || parent->type != LXB_DOM_NODE_TYPE_ELEMENT) return JS_NULL;
  return make_element(ctx, lxb_dom_interface_element(parent));
}

JSValue js_el_get_children(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el) return JS_NewArray(ctx);
  return LiveCollectionBindings::makeChildren(ctx, el);
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
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NewInt32(ctx, 0);
  return JS_NewInt32(ctx, (int32_t)node->type);
}

JSValue js_el_get_nodeName(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NewString(ctx, "");
  switch (node->type) {
    case LXB_DOM_NODE_TYPE_ELEMENT:           return js_el_get_tagName(ctx, this_val);
    case LXB_DOM_NODE_TYPE_TEXT:              return JS_NewString(ctx, "#text");
    case LXB_DOM_NODE_TYPE_COMMENT:           return JS_NewString(ctx, "#comment");
    case LXB_DOM_NODE_TYPE_DOCUMENT:          return JS_NewString(ctx, "#document");
    case LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT: return JS_NewString(ctx, "#document-fragment");
    default:                                  return JS_NewString(ctx, "");
  }
}

JSValue js_el_get_nodeValue(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  if (node->type != LXB_DOM_NODE_TYPE_TEXT && node->type != LXB_DOM_NODE_TYPE_COMMENT) return JS_NULL;
  auto* cd = reinterpret_cast<lxb_dom_character_data_t*>(node);
  return JS_NewStringLen(ctx, reinterpret_cast<const char*>(cd->data.data), cd->data.length);
}

JSValue js_el_set_nodeValue(JSContext* ctx, JSValue this_val, JSValue val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_UNDEFINED;
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
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NewArray(ctx);
  return LiveCollectionBindings::makeChildNodes(ctx, node);
}

JSValue js_el_get_firstChild(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  return make_element(ctx, node->first_child);
}

JSValue js_el_get_lastChild(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  return make_element(ctx, node->last_child);
}

JSValue js_el_get_nextSibling(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  return make_element(ctx, node->next);
}

JSValue js_el_get_previousSibling(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  return make_element(ctx, node->prev);
}

JSValue js_el_get_parentNode(JSContext* ctx, JSValue this_val) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  return make_element(ctx, node->parent);
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

// Inserts `node` via `insert_one`, expanding DocumentFragments into their
// children first (per the DOM's "insert a node" algorithm — a fragment is
// never itself part of the resulting tree, only its children are moved in).
template <typename InsertOneFn>
std::vector<lxb_dom_node_t*> expand_and_insert(lxb_dom_node_t* node, InsertOneFn insert_one) {
  std::vector<lxb_dom_node_t*> inserted;
  if (node->type == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT) {
    while (node->first_child) {
      lxb_dom_node_t* child = node->first_child;
      lxb_dom_node_remove(child);
      insert_one(child);
      inserted.push_back(child);
    }
  } else {
    insert_one(node);
    inserted.push_back(node);
  }
  return inserted;
}

JSValue js_el_appendChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent_node = unwrap_node(ctx, this_val);
  if (!parent_node || argc < 1) return JS_NULL;
  auto* child_node = unwrap_node(ctx, argv[0]);
  if (!child_node) return JS_NULL;

  auto inserted = expand_and_insert(child_node, [&](lxb_dom_node_t* n) {
    lxb_dom_node_insert_child(parent_node, n);
  });

  auto* rctx = get_ctx(ctx);
  if (!inserted.empty() && rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    std::vector<void*> insertedVoid(inserted.begin(), inserted.end());
    rctx->mutation_observers->notifyChildList(ctx, parent_node,
        insertedVoid, {}, inserted.front()->prev, nullptr);
  }

  return JS_DupValue(ctx, argv[0]);
}

JSValue js_el_removeChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent_node = unwrap_node(ctx, this_val);
  if (!parent_node || argc < 1) return JS_NULL;
  auto* cn = unwrap_node(ctx, argv[0]);
  if (!cn) return JS_NULL;
  if (cn->parent == parent_node) {
    lxb_dom_node_t* prev_sib = cn->prev;
    lxb_dom_node_t* next_sib = cn->next;
    invalidate_node_cache(ctx, get_ctx(ctx), cn);
    lxb_dom_node_remove(cn);

    auto* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
      rctx->mutation_observers->notifyChildList(ctx, parent_node,
          {}, { cn }, prev_sib, next_sib);
    }
  }
  return JS_DupValue(ctx, argv[0]);
}

JSValue js_el_insertBefore(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  auto* new_node = unwrap_node(ctx, argv[0]);
  if (!new_node) return JS_NULL;

  lxb_dom_node_t* parent_node = nullptr;
  std::vector<lxb_dom_node_t*> inserted;

  if (argc < 2 || JS_IsNull(argv[1])) {
    parent_node = unwrap_node(ctx, this_val);
    if (parent_node) {
      inserted = expand_and_insert(new_node, [&](lxb_dom_node_t* n) {
        lxb_dom_node_insert_child(parent_node, n);
      });
    }
  } else {
    auto* ref = unwrap_node(ctx, argv[1]);
    if (ref) {
      lxb_dom_node_t* receiver_node = unwrap_node(ctx, this_val);
      if (!receiver_node || ref->parent != receiver_node) {
        JS_ThrowTypeError(ctx, "NotFoundError: reference node is not a child of this node");
        return JS_EXCEPTION;
      }
      parent_node = receiver_node;
      inserted = expand_and_insert(new_node, [&](lxb_dom_node_t* n) {
        lxb_dom_node_insert_before(ref, n);
      });
    }
  }

  if (parent_node && !inserted.empty()) {
    auto* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
      std::vector<void*> insertedVoid(inserted.begin(), inserted.end());
      rctx->mutation_observers->notifyChildList(ctx, parent_node,
          insertedVoid, {}, inserted.front()->prev, inserted.back()->next);
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
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_NULL;
  bool deep = argc >= 1 && JS_ToBool(ctx, argv[0]) > 0;
  lxb_dom_node_t* clone = lxb_dom_node_clone(node, deep);
  return make_element(ctx, clone);
}

JSValue js_el_contains(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node || argc < 1) return JS_FALSE;
  auto* other = unwrap_node(ctx, argv[0]);
  if (!other) return JS_FALSE;
  for (lxb_dom_node_t* n = other; n; n = n->parent) {
    if (n == node) return JS_TRUE;
  }
  return JS_FALSE;
}

JSValue js_el_replaceChild(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent_node = unwrap_node(ctx, this_val);
  if (!parent_node || argc < 2) return JS_NULL;
  auto* new_node = unwrap_node(ctx, argv[0]);
  auto* old_node = unwrap_node(ctx, argv[1]);
  if (!new_node || !old_node) return JS_NULL;
  if (old_node->parent != parent_node) {
    JS_ThrowTypeError(ctx, "NotFoundError: the node to be replaced is not a child of this node");
    return JS_EXCEPTION;
  }

  lxb_dom_node_t* prev_sib = old_node->prev;
  lxb_dom_node_t* next_sib = old_node->next;

  auto inserted = expand_and_insert(new_node, [&](lxb_dom_node_t* n) {
    lxb_dom_node_insert_before(old_node, n);
  });

  auto* rctx = get_ctx(ctx);
  invalidate_node_cache(ctx, rctx, old_node);
  lxb_dom_node_remove(old_node);

  if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    std::vector<void*> insertedVoid(inserted.begin(), inserted.end());
    rctx->mutation_observers->notifyChildList(ctx, parent_node,
        insertedVoid, { old_node }, prev_sib, next_sib);
  }

  return JS_DupValue(ctx, argv[1]); // returns the replaced (old) child, per spec
}

lxb_dom_node_t* js_to_node_or_text(JSContext* ctx, JSValue val) {
  auto* node = unwrap_node(ctx, val);
  if (node) return node;
  const char* str = JS_ToCString(ctx, val);
  if (!str) return nullptr;
  void* text = get_doc(ctx)->createTextNode(str);
  JS_FreeCString(ctx, str);
  return static_cast<lxb_dom_node_t*>(text);
}

JSValue js_el_before(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node || !node->parent) return JS_UNDEFINED;
  lxb_dom_node_t* parent = node->parent;

  std::vector<void*> inserted;
  for (int i = 0; i < argc; i++) {
    lxb_dom_node_t* n = js_to_node_or_text(ctx, argv[i]);
    if (!n) continue;
    auto batch = expand_and_insert(n, [&](lxb_dom_node_t* one) { lxb_dom_node_insert_before(node, one); });
    inserted.insert(inserted.end(), batch.begin(), batch.end());
  }

  auto* rctx = get_ctx(ctx);
  if (!inserted.empty() && rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, parent, inserted, {},
        static_cast<lxb_dom_node_t*>(inserted.front())->prev, node);
  }
  return JS_UNDEFINED;
}

JSValue js_el_after(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node || !node->parent) return JS_UNDEFINED;
  lxb_dom_node_t* parent = node->parent;
  lxb_dom_node_t* ref = node->next;

  std::vector<void*> inserted;
  for (int i = 0; i < argc; i++) {
    lxb_dom_node_t* n = js_to_node_or_text(ctx, argv[i]);
    if (!n) continue;
    auto batch = expand_and_insert(n, [&](lxb_dom_node_t* one) {
      if (ref) lxb_dom_node_insert_before(ref, one); else lxb_dom_node_insert_child(parent, one);
    });
    inserted.insert(inserted.end(), batch.begin(), batch.end());
  }

  auto* rctx = get_ctx(ctx);
  if (!inserted.empty() && rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, parent, inserted, {}, node, ref);
  }
  return JS_UNDEFINED;
}

JSValue js_el_replaceWith(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node || !node->parent) return JS_UNDEFINED;
  lxb_dom_node_t* parent = node->parent;
  lxb_dom_node_t* prev_sib = node->prev;
  lxb_dom_node_t* next_sib = node->next;

  std::vector<void*> inserted;
  for (int i = 0; i < argc; i++) {
    lxb_dom_node_t* n = js_to_node_or_text(ctx, argv[i]);
    if (!n) continue;
    auto batch = expand_and_insert(n, [&](lxb_dom_node_t* one) { lxb_dom_node_insert_before(node, one); });
    inserted.insert(inserted.end(), batch.begin(), batch.end());
  }

  auto* rctx = get_ctx(ctx);
  invalidate_node_cache(ctx, rctx, node);
  lxb_dom_node_remove(node);

  if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, parent, inserted, { node }, prev_sib, next_sib);
  }
  return JS_UNDEFINED;
}

JSValue js_el_append(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent_node = unwrap_node(ctx, this_val);
  if (!parent_node) return JS_UNDEFINED;

  std::vector<void*> inserted;
  for (int i = 0; i < argc; i++) {
    lxb_dom_node_t* n = js_to_node_or_text(ctx, argv[i]);
    if (!n) continue;
    auto batch = expand_and_insert(n, [&](lxb_dom_node_t* one) { lxb_dom_node_insert_child(parent_node, one); });
    inserted.insert(inserted.end(), batch.begin(), batch.end());
  }

  auto* rctx = get_ctx(ctx);
  if (!inserted.empty() && rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, parent_node, inserted, {},
        static_cast<lxb_dom_node_t*>(inserted.front())->prev, nullptr);
  }
  return JS_UNDEFINED;
}

JSValue js_el_prepend(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* parent_node = unwrap_node(ctx, this_val);
  if (!parent_node) return JS_UNDEFINED;
  lxb_dom_node_t* ref = parent_node->first_child;

  std::vector<void*> inserted;
  for (int i = 0; i < argc; i++) {
    lxb_dom_node_t* n = js_to_node_or_text(ctx, argv[i]);
    if (!n) continue;
    auto batch = expand_and_insert(n, [&](lxb_dom_node_t* one) {
      if (ref) lxb_dom_node_insert_before(ref, one); else lxb_dom_node_insert_child(parent_node, one);
    });
    inserted.insert(inserted.end(), batch.begin(), batch.end());
  }

  auto* rctx = get_ctx(ctx);
  if (!inserted.empty() && rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, parent_node, inserted, {}, nullptr, ref);
  }
  return JS_UNDEFINED;
}

JSValue js_el_closest(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;

  JSValue result = JS_NULL;
  for (lxb_dom_node_t* node = lxb_dom_interface_node(el);
       node && node->type == LXB_DOM_NODE_TYPE_ELEMENT;
       node = node->parent) {
    if (get_doc(ctx)->matchesSelector(lxb_dom_interface_element(node), sel)) {
      result = make_element(ctx, node);
      break;
    }
  }
  JS_FreeCString(ctx, sel);
  return result;
}

JSValue js_el_insertAdjacentHTML(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 2) return JS_UNDEFINED;
  const char* position = JS_ToCString(ctx, argv[0]);
  const char* html = JS_ToCString(ctx, argv[1]);
  if (!position || !html) {
    if (position) JS_FreeCString(ctx, position);
    if (html) JS_FreeCString(ctx, html);
    return JS_UNDEFINED;
  }
  std::string pos(position);
  JS_FreeCString(ctx, position);

  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  auto parsed = get_doc(ctx)->parseFragmentNodes(el, html);
  JS_FreeCString(ctx, html);
  if (parsed.empty()) return JS_UNDEFINED;

  lxb_dom_node_t* parent = nullptr;
  lxb_dom_node_t* prev_sib = nullptr;
  lxb_dom_node_t* next_sib = nullptr;

  if (pos == "beforebegin") {
    parent = node->parent;
    if (!parent) return JS_UNDEFINED;
    prev_sib = node->prev;
    for (void* n : parsed) lxb_dom_node_insert_before(node, static_cast<lxb_dom_node_t*>(n));
    next_sib = node;
  } else if (pos == "afterbegin") {
    parent = node;
    lxb_dom_node_t* ref = node->first_child;
    for (void* n : parsed) {
      auto* dn = static_cast<lxb_dom_node_t*>(n);
      if (ref) lxb_dom_node_insert_before(ref, dn); else lxb_dom_node_insert_child(node, dn);
    }
    next_sib = ref;
  } else if (pos == "beforeend") {
    parent = node;
    prev_sib = node->last_child;
    for (void* n : parsed) lxb_dom_node_insert_child(node, static_cast<lxb_dom_node_t*>(n));
  } else if (pos == "afterend") {
    parent = node->parent;
    if (!parent) return JS_UNDEFINED;
    prev_sib = node;
    lxb_dom_node_t* ref = node->next;
    for (void* n : parsed) {
      auto* dn = static_cast<lxb_dom_node_t*>(n);
      if (ref) lxb_dom_node_insert_before(ref, dn); else lxb_dom_node_insert_child(parent, dn);
    }
    next_sib = ref;
  } else {
    for (void* n : parsed) lxb_dom_node_destroy_deep(static_cast<lxb_dom_node_t*>(n));
    JS_ThrowTypeError(ctx, "SyntaxError: invalid insertAdjacentHTML position '%s'", pos.c_str());
    return JS_EXCEPTION;
  }

  auto* rctx = get_ctx(ctx);
  if (rctx && rctx->mutation_observers && !rctx->mutation_observers->empty()) {
    rctx->mutation_observers->notifyChildList(ctx, parent, parsed, {}, prev_sib, next_sib);
  }
  return JS_UNDEFINED;
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
  JSValue result = LiveCollectionBindings::makeBySelector(ctx, el, classNames_to_selector(names));
  JS_FreeCString(ctx, names);
  return result;
}

JSValue js_el_getElementsByTagName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_NewArray(ctx);
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NewArray(ctx);
  JSValue result = LiveCollectionBindings::makeBySelector(ctx, el, tag);
  JS_FreeCString(ctx, tag);
  return result;
}

JSValue js_el_getAttributeNames(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* el = unwrap_element(ctx, this_val);
  JSValue arr = JS_NewArray(ctx);
  if (!el) return arr;
  uint32_t idx = 0;
  for (auto* attr = lxb_dom_element_first_attribute(el); attr; attr = lxb_dom_element_next_attribute(attr)) {
    size_t len = 0;
    const lxb_char_t* name = lxb_dom_attr_qualified_name(attr, &len);
    JS_SetPropertyUint32(ctx, arr, idx++, JS_NewStringLen(ctx, reinterpret_cast<const char*>(name), len));
  }
  return arr;
}

// NamedNodeMap-like: a snapshot array of {name, value} pairs rather than a
// live spec-accurate NamedNodeMap, so it's plain-Array iterable for free.
JSValue js_el_get_attributes(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  JSValue arr = JS_NewArray(ctx);
  if (!el) return arr;
  uint32_t idx = 0;
  for (auto* attr = lxb_dom_element_first_attribute(el); attr; attr = lxb_dom_element_next_attribute(attr)) {
    size_t nlen = 0, vlen = 0;
    const lxb_char_t* name = lxb_dom_attr_qualified_name(attr, &nlen);
    const lxb_char_t* value = lxb_dom_attr_value(attr, &vlen);
    JSValue entry = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, entry, "name", JS_NewStringLen(ctx, reinterpret_cast<const char*>(name), nlen));
    JS_SetPropertyStr(ctx, entry, "value",
        value ? JS_NewStringLen(ctx, reinterpret_cast<const char*>(value), vlen) : JS_NewString(ctx, ""));
    JS_SetPropertyUint32(ctx, arr, idx++, entry);
  }
  return arr;
}

JSValue js_el_toggleAttribute(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || argc < 1) return JS_FALSE;
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_FALSE;
  size_t name_len = strlen(name);
  auto* attr_name = reinterpret_cast<const lxb_char_t*>(name);

  bool has = lxb_dom_element_has_attribute(el, attr_name, name_len);
  bool force_present = (argc >= 2 && !JS_IsUndefined(argv[1])) ? JS_ToBool(ctx, argv[1]) > 0 : !has;

  auto* rctx = get_ctx(ctx);
  bool has_obs = rctx && rctx->mutation_observers && !rctx->mutation_observers->empty();
  std::optional<std::string> old_val;
  if (has_obs && has && rctx->mutation_observers->hasAttributeOldValueObserver()) {
    size_t len = 0;
    const lxb_char_t* v = lxb_dom_element_get_attribute(el, attr_name, name_len, &len);
    if (v) old_val = std::string(reinterpret_cast<const char*>(v), len);
  }

  bool changed = false;
  if (force_present && !has) {
    lxb_dom_element_set_attribute(el, attr_name, name_len, reinterpret_cast<const lxb_char_t*>(""), 0);
    changed = true;
  } else if (!force_present && has) {
    lxb_dom_element_remove_attribute(el, attr_name, name_len);
    changed = true;
  }

  if (changed && has_obs) {
    rctx->mutation_observers->notifyAttribute(ctx, lxb_dom_interface_node(el), std::string(name), old_val);
  }

  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, force_present);
}

JSValue js_el_isSameNode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node || argc < 1) return JS_FALSE;
  return JS_NewBool(ctx, unwrap_node(ctx, argv[0]) == node);
}

bool nodes_equal(lxb_dom_node_t* a, lxb_dom_node_t* b) {
  if (a->type != b->type) return false;

  if (a->type == LXB_DOM_NODE_TYPE_ELEMENT) {
    auto* ea = lxb_dom_interface_element(a);
    auto* eb = lxb_dom_interface_element(b);

    size_t la = 0, lb = 0;
    const lxb_char_t* na = lxb_dom_element_local_name(ea, &la);
    const lxb_char_t* nb = lxb_dom_element_local_name(eb, &lb);
    if (la != lb || memcmp(na, nb, la) != 0) return false;

    size_t count_a = 0, count_b = 0;
    for (auto* at = lxb_dom_element_first_attribute(ea); at; at = lxb_dom_element_next_attribute(at)) count_a++;
    for (auto* at = lxb_dom_element_first_attribute(eb); at; at = lxb_dom_element_next_attribute(at)) count_b++;
    if (count_a != count_b) return false;

    for (auto* at = lxb_dom_element_first_attribute(ea); at; at = lxb_dom_element_next_attribute(at)) {
      size_t nlen = 0, vlen = 0;
      const lxb_char_t* name = lxb_dom_attr_qualified_name(at, &nlen);
      const lxb_char_t* val = lxb_dom_attr_value(at, &vlen);
      size_t blen = 0;
      const lxb_char_t* bval = lxb_dom_element_get_attribute(eb, name, nlen, &blen);
      if (!bval || vlen != blen || (vlen > 0 && memcmp(val, bval, vlen) != 0)) return false;
    }
  } else if (a->type == LXB_DOM_NODE_TYPE_TEXT || a->type == LXB_DOM_NODE_TYPE_COMMENT) {
    auto* ca = reinterpret_cast<lxb_dom_character_data_t*>(a);
    auto* cb = reinterpret_cast<lxb_dom_character_data_t*>(b);
    if (ca->data.length != cb->data.length) return false;
    if (ca->data.length > 0 && memcmp(ca->data.data, cb->data.data, ca->data.length) != 0) return false;
  }

  lxb_dom_node_t* ca = a->first_child;
  lxb_dom_node_t* cb = b->first_child;
  while (ca && cb) {
    if (!nodes_equal(ca, cb)) return false;
    ca = ca->next;
    cb = cb->next;
  }
  return ca == nullptr && cb == nullptr;
}

JSValue js_el_isEqualNode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* node = unwrap_node(ctx, this_val);
  if (!node || argc < 1) return JS_FALSE;
  auto* other = unwrap_node(ctx, argv[0]);
  if (!other) return JS_FALSE;
  return JS_NewBool(ctx, nodes_equal(node, other));
}

} // namespace

void ElementBindings::install(JSContext* ctx) {
  if (js_element_class_id == 0) JS_NewClassID(&js_element_class_id);
  if (js_node_class_id == 0)    JS_NewClassID(&js_node_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);
  JS_NewClass(JS_GetRuntime(ctx), js_node_class_id,    &js_node_class);

  ClassListBindings::install(ctx);
  DatasetBindings::install(ctx);
  StyleBindings::install(ctx);

  // ── Node proto: shared traversal + CharacterData methods ─────────────────
  JSValue node_proto = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, node_proto, "appendChild",  JS_NewCFunction(ctx, js_el_appendChild,  "appendChild",  1));
  JS_SetPropertyStr(ctx, node_proto, "removeChild",  JS_NewCFunction(ctx, js_el_removeChild,  "removeChild",  1));
  JS_SetPropertyStr(ctx, node_proto, "insertBefore", JS_NewCFunction(ctx, js_el_insertBefore, "insertBefore", 2));
  JS_SetPropertyStr(ctx, node_proto, "replaceChild", JS_NewCFunction(ctx, js_el_replaceChild, "replaceChild", 2));
  JS_SetPropertyStr(ctx, node_proto, "cloneNode",    JS_NewCFunction(ctx, js_el_cloneNode,    "cloneNode",    1));
  JS_SetPropertyStr(ctx, node_proto, "contains",     JS_NewCFunction(ctx, js_el_contains,     "contains",     1));
  JS_SetPropertyStr(ctx, node_proto, "before",       JS_NewCFunction(ctx, js_el_before,       "before",       0));
  JS_SetPropertyStr(ctx, node_proto, "after",        JS_NewCFunction(ctx, js_el_after,        "after",        0));
  JS_SetPropertyStr(ctx, node_proto, "replaceWith",  JS_NewCFunction(ctx, js_el_replaceWith,  "replaceWith",  0));
  JS_SetPropertyStr(ctx, node_proto, "isSameNode",   JS_NewCFunction(ctx, js_el_isSameNode,   "isSameNode",   1));
  JS_SetPropertyStr(ctx, node_proto, "isEqualNode",  JS_NewCFunction(ctx, js_el_isEqualNode,  "isEqualNode",  1));

  define_prop(ctx, node_proto, "nodeType",         js_el_get_nodeType,        nullptr);
  define_prop(ctx, node_proto, "nodeName",         js_el_get_nodeName,        nullptr);
  define_prop(ctx, node_proto, "nodeValue",        js_el_get_nodeValue,       js_el_set_nodeValue);
  define_prop(ctx, node_proto, "textContent",      js_el_get_textContent,     js_el_set_textContent);
  define_prop(ctx, node_proto, "childNodes",       js_el_get_childNodes,      nullptr);
  define_prop(ctx, node_proto, "firstChild",       js_el_get_firstChild,      nullptr);
  define_prop(ctx, node_proto, "lastChild",        js_el_get_lastChild,       nullptr);
  define_prop(ctx, node_proto, "nextSibling",      js_el_get_nextSibling,     nullptr);
  define_prop(ctx, node_proto, "previousSibling",  js_el_get_previousSibling, nullptr);
  define_prop(ctx, node_proto, "parentNode",       js_el_get_parentNode,      nullptr);
  define_prop(ctx, node_proto, "parentElement",    js_el_get_parentElement,   nullptr);

  JS_SetClassProto(ctx, js_node_class_id, JS_DupValue(ctx, node_proto));

  // ── Element proto: inherits Node, adds Element-specific APIs ─────────────
  JSValue proto = JS_NewObjectProto(ctx, node_proto);
  JS_FreeValue(ctx, node_proto);

  JS_SetPropertyStr(ctx, proto, "getAttribute",           JS_NewCFunction(ctx, js_el_getAttribute,           "getAttribute",           1));
  JS_SetPropertyStr(ctx, proto, "setAttribute",           JS_NewCFunction(ctx, js_el_setAttribute,           "setAttribute",           2));
  JS_SetPropertyStr(ctx, proto, "removeAttribute",        JS_NewCFunction(ctx, js_el_removeAttribute,        "removeAttribute",        1));
  JS_SetPropertyStr(ctx, proto, "hasAttribute",           JS_NewCFunction(ctx, js_el_hasAttribute,           "hasAttribute",           1));
  JS_SetPropertyStr(ctx, proto, "toggleAttribute",        JS_NewCFunction(ctx, js_el_toggleAttribute,        "toggleAttribute",        2));
  JS_SetPropertyStr(ctx, proto, "getAttributeNames",      JS_NewCFunction(ctx, js_el_getAttributeNames,      "getAttributeNames",      0));
  JS_SetPropertyStr(ctx, proto, "remove",                 JS_NewCFunction(ctx, js_el_remove,                 "remove",                 0));
  JS_SetPropertyStr(ctx, proto, "matches",                JS_NewCFunction(ctx, js_el_matches,                "matches",                1));
  JS_SetPropertyStr(ctx, proto, "querySelector",          JS_NewCFunction(ctx, js_el_querySelector,          "querySelector",          1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll",       JS_NewCFunction(ctx, js_el_querySelectorAll,       "querySelectorAll",       1));
  JS_SetPropertyStr(ctx, proto, "getElementsByClassName", JS_NewCFunction(ctx, js_el_getElementsByClassName, "getElementsByClassName", 1));
  JS_SetPropertyStr(ctx, proto, "getElementsByTagName",   JS_NewCFunction(ctx, js_el_getElementsByTagName,   "getElementsByTagName",   1));
  JS_SetPropertyStr(ctx, proto, "closest",                JS_NewCFunction(ctx, js_el_closest,                "closest",                1));
  JS_SetPropertyStr(ctx, proto, "insertAdjacentHTML",     JS_NewCFunction(ctx, js_el_insertAdjacentHTML,     "insertAdjacentHTML",     2));
  JS_SetPropertyStr(ctx, proto, "append",                 JS_NewCFunction(ctx, js_el_append,                 "append",                 0));
  JS_SetPropertyStr(ctx, proto, "prepend",                JS_NewCFunction(ctx, js_el_prepend,                "prepend",                0));

  define_prop(ctx, proto, "tagName",                js_el_get_tagName,             nullptr);
  define_prop(ctx, proto, "id",                     js_el_get_id,                  js_el_set_id);
  define_prop(ctx, proto, "className",              js_el_get_className,           js_el_set_className);
  define_prop(ctx, proto, "innerHTML",              js_el_get_innerHTML,           js_el_set_innerHTML);
  define_prop(ctx, proto, "outerHTML",              js_el_get_outerHTML,           nullptr);
  define_prop(ctx, proto, "children",               js_el_get_children,            nullptr);
  define_prop(ctx, proto, "firstElementChild",      js_el_get_firstElementChild,   nullptr);
  define_prop(ctx, proto, "lastElementChild",       js_el_get_lastElementChild,    nullptr);
  define_prop(ctx, proto, "nextElementSibling",     js_el_get_nextElementSibling,  nullptr);
  define_prop(ctx, proto, "previousElementSibling", js_el_get_prevElementSibling,  nullptr);
  define_prop(ctx, proto, "classList",              js_el_get_classList,           nullptr);
  define_prop(ctx, proto, "dataset",                js_el_get_dataset,             nullptr);
  define_prop(ctx, proto, "style",                  js_el_get_style,               nullptr);
  define_prop(ctx, proto, "childElementCount",      js_el_get_childElementCount,   nullptr);
  define_prop(ctx, proto, "attributes",              js_el_get_attributes,          nullptr);

  JS_SetClassProto(ctx, js_element_class_id, proto);

  JSValue node_proto_ref    = JS_GetClassProto(ctx, js_node_class_id);
  JSValue element_proto_ref = JS_GetClassProto(ctx, js_element_class_id);

  JS_FreeValue(ctx, define_global_constructor(ctx, "Node", node_proto_ref));
  JSValue element_ctor = define_global_constructor(ctx, "Element", element_proto_ref);
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "HTMLElement", element_ctor);
  JS_FreeValue(ctx, global);

  JS_FreeValue(ctx, node_proto_ref);
  JS_FreeValue(ctx, element_proto_ref);
}

} // namespace margelo::nitro::nitrojsdom
