#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers a minimal CSSOM: globalThis.CSSRule/CSSStyleRule/CSSStyleSheet,
// Element.prototype.sheet (tag-checked for "style"), document.styleSheets,
// and globalThis.getComputedStyle (inline-style + tag-default-display only,
// no real cascade — see the bootstrap script for the exact fallback rules).
//
// Rules are split by a hand-rolled top-level brace scanner rather than
// Lexbor's CSS rule/declaration AST (lxb_css_rule_t) — mirrors how
// StyleBindings.cpp already treats inline "style" text as informally-parsed
// property:value pairs rather than integrating Lexbor's typed CSS value
// engine. At-rules (@media, @keyframes, ...) are captured as opaque stub
// rules (cssText only, no nested-rule access) rather than fully parsed.
//
// Must run after ElementBindings (globalThis.Element) and DocumentBindings
// (globalThis.document).
struct CSSOMBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
