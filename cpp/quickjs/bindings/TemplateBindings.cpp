#include "TemplateBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <lexbor/dom/dom.h>
#include <cstring>
#include <cctype>

namespace margelo::nitro::nitrojsdom {

namespace {

bool is_template_element(lxb_dom_element_t* el) {
  size_t len = 0;
  const lxb_char_t* name = lxb_dom_element_local_name(el, &len);
  if (!name || len != 8) return false;
  for (size_t i = 0; i < len; i++) {
    if (std::tolower(name[i]) != "template"[i]) return false;
  }
  return true;
}

JSValue js_el_get_content(JSContext* ctx, JSValue this_val) {
  auto* el = unwrap_element(ctx, this_val);
  if (!el || !is_template_element(el)) return JS_UNDEFINED;
  return make_element(ctx, get_doc(ctx)->templateContent(el));
}

} // namespace

void TemplateBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue element_ctor = JS_GetPropertyStr(ctx, global, "Element");
  JSValue element_proto = JS_GetPropertyStr(ctx, element_ctor, "prototype");

  define_prop(ctx, element_proto, "content", js_el_get_content, nullptr);

  JS_FreeValue(ctx, element_proto);
  JS_FreeValue(ctx, element_ctor);
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
