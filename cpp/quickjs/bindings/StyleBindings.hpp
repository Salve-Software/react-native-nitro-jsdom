#pragma once

// element.style, a CSSStyleDeclaration-like object mirroring the `style`
// attribute. Same exotic-property technique as DatasetBindings: individual
// longhand properties (backgroundColor) go through get/define/delete_property
// against the parsed `style` attribute text; cssText/setProperty/etc. live on
// the class prototype, a plain object that doesn't re-enter the exotic hooks.

#include "quickjs.h"

struct lxb_dom_element;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace margelo::nitro::nitrojsdom {

class StyleBindings {
public:
  static void install(JSContext* ctx);
  static JSValue make(JSContext* ctx, lxb_dom_element_t* el);
};

} // namespace margelo::nitro::nitrojsdom
