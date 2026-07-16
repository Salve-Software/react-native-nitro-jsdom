#pragma once

// Registers the "Element" JS class and its prototype: attributes, classList,
// dataset, style, generic Node traversal (nodeType/nodeName/childNodes/...),
// and DOM mutation/query methods (appendChild, cloneNode, querySelector, ...).
//
// addEventListener/removeEventListener/dispatchEvent are attached separately
// by EventBindings::install(), which must run after this one.

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class ElementBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
