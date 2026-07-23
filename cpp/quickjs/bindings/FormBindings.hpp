#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.FormData, Element.prototype.requestSubmit/submit, and
// the Constraint Validation API (globalThis.ValidityState,
// Element.prototype.validity/willValidate/validationMessage/checkValidity()/
// reportValidity()/setCustomValidity()). Tag-checked for "form"/"input"/
// "select"/"textarea" internally, mirroring ElementBindings' value/checked
// pattern. Pure JS on top of the already-exposed getAttribute/value/checked/
// querySelectorAll/Event primitives — no native code needed. Must run after
// ElementBindings (for globalThis.Element) and EventBindings (for
// globalThis.Event).
struct FormBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
