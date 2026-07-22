#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.TextEncoder/TextDecoder (UTF-8 only — the vast
// majority of real-world usage — implemented in pure JS since encoding a
// JS string's UTF-16 code units to UTF-8 bytes needs no native primitive).
struct TextEncodingBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
