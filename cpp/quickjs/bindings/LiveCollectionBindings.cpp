#include "LiveCollectionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <lexbor/dom/dom.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_live_collection_class_id = 0;

enum class LiveKind { CHILDREN, CHILD_NODES, SELECTOR };

struct LiveCollectionData {
  void* owner;
  LiveKind kind;
  std::string selector;
};

LiveCollectionData* unwrap_live(JSContext*, JSValue val) {
  return static_cast<LiveCollectionData*>(JS_GetOpaque(val, js_live_collection_class_id));
}

std::vector<void*> compute_results(JSContext* ctx, LiveCollectionData* data) {
  switch (data->kind) {
    case LiveKind::CHILDREN: {
      std::vector<void*> out;
      auto* node = static_cast<lxb_dom_node_t*>(data->owner);
      for (auto* c = node->first_child; c; c = c->next)
        if (c->type == LXB_DOM_NODE_TYPE_ELEMENT) out.push_back(c);
      return out;
    }
    case LiveKind::CHILD_NODES: {
      std::vector<void*> out;
      auto* node = static_cast<lxb_dom_node_t*>(data->owner);
      for (auto* c = node->first_child; c; c = c->next) out.push_back(c);
      return out;
    }
    case LiveKind::SELECTOR:
      if (data->owner) return get_doc(ctx)->querySelectorAllFromEl(data->owner, data->selector);
      return get_doc(ctx)->querySelectorAll_el(data->selector);
  }
  return {};
}

bool prop_is_index(const std::string& key, size_t& out_idx) {
  if (key.empty()) return false;
  for (char c : key) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  out_idx = static_cast<size_t>(std::strtoul(key.c_str(), nullptr, 10));
  return true;
}

int js_livecoll_get_own_property(JSContext* ctx, JSPropertyDescriptor* desc, JSValue obj, JSAtom prop) {
  auto* data = unwrap_live(ctx, obj);
  if (!data) return 0;

  JSValue key_val = JS_AtomToString(ctx, prop);
  const char* key_c = JS_ToCString(ctx, key_val);
  std::string key = key_c ? key_c : "";
  if (key_c) JS_FreeCString(ctx, key_c);
  JS_FreeValue(ctx, key_val);

  if (key == "length") {
    if (desc) {
      desc->flags = JS_PROP_ENUMERABLE;
      desc->value = JS_NewInt32(ctx, static_cast<int32_t>(compute_results(ctx, data).size()));
      desc->getter = JS_UNDEFINED;
      desc->setter = JS_UNDEFINED;
    }
    return 1;
  }

  size_t idx;
  if (prop_is_index(key, idx)) {
    auto results = compute_results(ctx, data);
    if (idx < results.size()) {
      if (desc) {
        desc->flags = JS_PROP_ENUMERABLE;
        desc->value = make_element(ctx, results[idx]);
        desc->getter = JS_UNDEFINED;
        desc->setter = JS_UNDEFINED;
      }
      return 1;
    }
  }

  return 0;
}

int js_livecoll_get_own_property_names(JSContext* ctx, JSPropertyEnum** ptab, uint32_t* plen, JSValue obj) {
  *ptab = nullptr;
  *plen = 0;
  auto* data = unwrap_live(ctx, obj);
  if (!data) return 0;

  auto results = compute_results(ctx, data);
  auto* tab = static_cast<JSPropertyEnum*>(js_malloc(ctx, sizeof(JSPropertyEnum) * (results.size() + 1)));
  if (!tab) return -1;

  for (size_t i = 0; i < results.size(); i++) {
    tab[i].is_enumerable = 1;
    tab[i].atom = JS_NewAtom(ctx, std::to_string(i).c_str());
  }
  tab[results.size()].is_enumerable = 0;
  tab[results.size()].atom = JS_NewAtom(ctx, "length");

  *ptab = tab;
  *plen = static_cast<uint32_t>(results.size() + 1);
  return 0;
}

JSClassExoticMethods js_livecoll_exotic = {
  .get_own_property       = js_livecoll_get_own_property,
  .get_own_property_names = js_livecoll_get_own_property_names,
};

void js_livecoll_finalizer(JSRuntime*, JSValue val) {
  auto* data = static_cast<LiveCollectionData*>(JS_GetOpaque(val, js_live_collection_class_id));
  delete data;
}

JSClassDef js_livecoll_class = { "LiveCollection", .finalizer = js_livecoll_finalizer, .exotic = &js_livecoll_exotic };

const char* kIteratorBootstrapScript = R"JS(
(function() {
  var proto = globalThis.__LiveCollectionProto;
  proto[Symbol.iterator] = function() {
    var i = 0;
    var self = this;
    return {
      next: function() {
        return i < self.length ? { value: self[i++], done: false } : { value: undefined, done: true };
      }
    };
  };

  proto.forEach = function(callback, thisArg) {
    for (var i = 0; i < this.length; i++) callback.call(thisArg, this[i], i, this);
  };
  delete globalThis.__LiveCollectionProto;
})();
)JS";

JSValue make(JSContext* ctx, void* owner, LiveKind kind, const std::string& selector) {
  auto* data = new LiveCollectionData{ owner, kind, selector };
  JSValue obj = JS_NewObjectClass(ctx, js_live_collection_class_id);
  JS_SetOpaque(obj, data);
  return obj;
}

} // namespace

void LiveCollectionBindings::install(JSContext* ctx) {
  if (js_live_collection_class_id == 0) JS_NewClassID(&js_live_collection_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_live_collection_class_id, &js_livecoll_class);

  JSValue proto = JS_NewObject(ctx);
  JS_SetClassProto(ctx, js_live_collection_class_id, JS_DupValue(ctx, proto));

  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "__LiveCollectionProto", proto);
  JS_FreeValue(ctx, global);

  JSValue result = JS_Eval(ctx, kIteratorBootstrapScript, strlen(kIteratorBootstrapScript),
                            "<live-collection-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) JS_FreeValue(ctx, JS_GetException(ctx));
  JS_FreeValue(ctx, result);
}

JSValue LiveCollectionBindings::makeChildren(JSContext* ctx, void* element) {
  return make(ctx, element, LiveKind::CHILDREN, "");
}

JSValue LiveCollectionBindings::makeChildNodes(JSContext* ctx, void* node) {
  return make(ctx, node, LiveKind::CHILD_NODES, "");
}

JSValue LiveCollectionBindings::makeBySelector(JSContext* ctx, void* owner_or_null, const std::string& selector) {
  return make(ctx, owner_or_null, LiveKind::SELECTOR, selector);
}

} // namespace margelo::nitro::nitrojsdom
