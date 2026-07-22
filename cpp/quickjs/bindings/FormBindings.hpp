#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.FormData and Element.prototype.requestSubmit/submit
// (tag-checked for "form" internally, mirroring ElementBindings' value/checked
// pattern). Pure JS on top of the already-exposed querySelectorAll/Event
// primitives — no native code needed. Must run after ElementBindings (for
// globalThis.Element) and EventBindings (for globalThis.Event).
struct FormBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
