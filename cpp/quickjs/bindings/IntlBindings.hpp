#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers a pure-JS globalThis.Intl.NumberFormat/DateTimeFormat (no ICU —
// QuickJS ships none). Locale data is a small hand-built table (en + pt, the
// two this project's users actually need), not real CLDR data. Also rewires
// Number.prototype.toLocaleString and Date.prototype.toLocaleString/
// toLocaleDateString/toLocaleTimeString to go through these instead of
// QuickJS's own locale-blind built-ins.
struct IntlBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
