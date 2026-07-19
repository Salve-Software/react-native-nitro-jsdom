#pragma once

// Event/CustomEvent constructors + prototype (preventDefault/stopPropagation/
// stopImmediatePropagation), and addEventListener/removeEventListener/
// dispatchEvent for both elements and the document object.
//
// install(ctx) must run after ElementBindings::install(ctx) (it fetches the
// Element class prototype via JS_GetClassProto to attach the event methods)
// and after DocumentBindings::install(ctx) (it fetches globalThis.document to
// attach addEventListener/dispatchEvent there too).

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class EventBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
