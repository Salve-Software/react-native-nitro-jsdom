#pragma once

// element.dataset (DOMStringMap), mirroring data-* attributes. Implemented as a
// QuickJS exotic-property class so `el.dataset.foo = 'x'` writes straight
// through to the `data-foo` attribute, rather than a one-shot snapshot object.

#include "quickjs.h"

struct lxb_dom_element;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace margelo::nitro::nitrojsdom {

class DatasetBindings {
public:
  static void install(JSContext* ctx);
  static JSValue make(JSContext* ctx, lxb_dom_element_t* el);
};

} // namespace margelo::nitro::nitrojsdom
