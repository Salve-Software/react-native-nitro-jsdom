#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers globalThis.Blob and globalThis.FileReader. Pure JS on top of
// TextEncoder/TextDecoder and btoa, so must run after TextEncodingBindings
// and WindowBindings.
struct BlobBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
