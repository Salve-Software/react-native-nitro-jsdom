#pragma once

// element.classList (DOMTokenList) — registered as its own QuickJS class since
// it's a distinct object handed out by the "classList" accessor, not a set of
// methods on Element itself.

#include "quickjs.h"
#include <string>

struct lxb_dom_element;
typedef struct lxb_dom_element lxb_dom_element_t;

namespace margelo::nitro::nitrojsdom {

class ClassListBindings {
public:
  // Registers the DOMTokenList class. Must run before make() is called.
  static void install(JSContext* ctx);

  // Wraps `el` as a classList instance (add/remove/contains/toggle/replace).
  static JSValue make(JSContext* ctx, lxb_dom_element_t* el);
};

// Shared with ElementBindings (className getter/setter) and StyleBindings-adjacent
// callers that need the raw `class` attribute value.
std::string get_class_attr(lxb_dom_element_t* el);
void set_class_attr(lxb_dom_element_t* el, const std::string& classes);

} // namespace margelo::nitro::nitrojsdom
