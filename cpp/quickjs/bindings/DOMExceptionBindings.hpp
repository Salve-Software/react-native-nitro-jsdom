#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.DOMException (extends Error, with the legacy numeric
// `.code` field). Installed first since every other binding module that
// throws a named DOM error (NotFoundError, SyntaxError, InvalidCharacterError,
// ...) depends on the constructor already existing on globalThis.
struct DOMExceptionBindings {
  static void install(JSContext* ctx);
};

// Constructs `new DOMException(message, name)` via the globalThis constructor
// and throws it (mirrors JS_ThrowTypeError's calling convention: always
// returns JS_EXCEPTION so call sites can `return throw_dom_exception(...)`).
JSValue throw_dom_exception(JSContext* ctx, const char* name, const char* message);

} // namespace margelo::nitro::nitrojsdom
