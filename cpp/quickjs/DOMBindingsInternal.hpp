#pragma once

// Internal header shared between DOMBindings.cpp and MutationObservers.cpp.
// Exposes the element class id and make_element helper so MutationObservers
// can wrap lxb_dom_node_t* pointers as JS Element objects without duplicating
// the class registration logic (which lives exclusively in DOMBindings.cpp).

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

extern JSClassID js_element_class_id;
JSValue make_element(JSContext* ctx, void* el);

} // namespace margelo::nitro::nitrojsdom
