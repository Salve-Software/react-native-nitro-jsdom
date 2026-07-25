#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers a pure-JS globalThis.Intl.NumberFormat/DateTimeFormat/
// PluralRules/RelativeTimeFormat (no ICU — QuickJS ships none). Locale data
// is a small hand-built table (en + pt, the two this project's users
// actually need), not real CLDR data. Also rewires Number.prototype.
// toLocaleString and Date.prototype.toLocaleString/toLocaleDateString/
// toLocaleTimeString to go through these instead of QuickJS's own
// locale-blind built-ins.
//
// RelativeTimeFormat's `style` option ('long'/'short'/'narrow') is accepted
// but 'short'/'narrow' render identically to 'long' — there's no abbreviated
// unit-name table, same trade-off DateTimeFormat already made collapsing
// timeStyle 'long'/'full'. `numeric: 'auto'` only has special phrasing
// ("yesterday", "next week", ..., plus "now"/"agora" for second: 0) for
// day/week/month/year/second — hour/minute/quarter always render
// numerically, matching real Intl's own behavior for units without a
// natural auto phrase. Auto phrasing only ever applies to exact supported
// integer values (e.g. -1/0/1) — a fractional value like 1.2 always falls
// through to numeric formatting rather than rounding into a phrase.
struct IntlBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
