#include "DatasetBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include <lexbor/dom/dom.h>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_dataset_class_id = 0;

lxb_dom_element_t* unwrap_dataset(JSContext* ctx, JSValue val) {
  return static_cast<lxb_dom_element_t*>(JS_GetOpaque(val, js_dataset_class_id));
}

std::string dataset_prop_to_attr(JSContext* ctx, JSAtom prop) {
  JSValue key_val = JS_AtomToString(ctx, prop);
  const char* key = JS_ToCString(ctx, key_val);
  JS_FreeValue(ctx, key_val);
  std::string attr_name = key ? ("data-" + camel_to_attr_suffix(key)) : "";
  if (key) JS_FreeCString(ctx, key);
  return attr_name;
}

int js_dataset_get_own_property(JSContext* ctx, JSPropertyDescriptor* desc, JSValue obj, JSAtom prop) {
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

int js_dataset_define_own_property(JSContext* ctx, JSValue this_obj, JSAtom prop, JSValue val,
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

int js_dataset_delete_property(JSContext* ctx, JSValue obj, JSAtom prop) {
  auto* el = unwrap_dataset(ctx, obj);
  if (!el) return 1;
  std::string attr_name = dataset_prop_to_attr(ctx, prop);
  if (!attr_name.empty()) {
    lxb_dom_element_remove_attribute(el,
        reinterpret_cast<const lxb_char_t*>(attr_name.data()), attr_name.size());
  }
  return 1;
}

int js_dataset_get_own_property_names(JSContext* ctx, JSPropertyEnum** ptab, uint32_t* plen, JSValue obj) {
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

JSClassExoticMethods js_dataset_exotic = {
  .get_own_property       = js_dataset_get_own_property,
  .get_own_property_names = js_dataset_get_own_property_names,
  .delete_property        = js_dataset_delete_property,
  .define_own_property    = js_dataset_define_own_property,
};

JSClassDef js_dataset_class = { "DOMStringMap", .finalizer = nullptr, .exotic = &js_dataset_exotic };

} // namespace

void DatasetBindings::install(JSContext* ctx) {
  JS_NewClassID(&js_dataset_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_dataset_class_id, &js_dataset_class);
}

JSValue DatasetBindings::make(JSContext* ctx, lxb_dom_element_t* el) {
  JSValue obj = JS_NewObjectClass(ctx, js_dataset_class_id);
  JS_SetOpaque(obj, el);
  return obj;
}

} // namespace margelo::nitro::nitrojsdom
