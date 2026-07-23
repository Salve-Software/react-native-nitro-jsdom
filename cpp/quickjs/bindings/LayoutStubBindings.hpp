#pragma once

// No layout/rendering engine backs this sandbox, so every geometry- and
// scroll-related API here is an inert stub rather than a real computation —
// same stance as CSSOMBindings' getComputedStyle and ElementBindings'
// getBoundingClientRect (both already all-zero without a cascade/layout
// pass). This file covers the rest of that surface: scrollIntoView/scrollTo/
// scrollBy/scroll (no-ops), offset*/client*/scroll* dimensions (0, except
// scrollTop/scrollLeft which are real per-element state — jsdom does the
// same), document.elementFromPoint/elementsFromPoint (null/[]), and
// ResizeObserver/IntersectionObserver (constructible, observe()/unobserve()/
// disconnect() no-op, callback never fires). jsdom itself doesn't expose
// Resize/IntersectionObserver at all; this project adds them as defensive
// "don't throw ReferenceError" stubs instead, matching the precedent set by
// window.history/window.getSelection().
//
// Must run after ElementBindings (globalThis.Element) and DocumentBindings
// (globalThis.document).

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

struct LayoutStubBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
