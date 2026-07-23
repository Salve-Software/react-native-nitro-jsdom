#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers a `content` getter on Element.prototype, tag-checked for
// "template" internally (mirroring FormBindings' tag-check pattern) —
// returns the lxb_dom_document_fragment_t Lexbor already allocates as
// lxb_html_template_element_t::content, for both parsed and
// document.createElement()'d <template> elements. No custom parsing logic:
// Lexbor's own HTML5 tree constructor already redirects a parsed
// <template>'s children into that fragment instead of the template's own
// light-DOM children (see packages/lexbor/.../tree/insertion_mode/in_template.c).
// LexborDocument::setInnerHTMLOnEl() mirrors that for template.innerHTML=.
//
// Scope: the returned fragment is a generic Node wrapper (like
// document.createDocumentFragment()), so it has childNodes/appendChild/etc.
// but not querySelector(All) — same limitation createDocumentFragment()
// already has. cloneNode() on a <template> does not specially deep-clone
// `.content` (relies on Lexbor's own node clone, not verified against spec
// for this case) — undocumented/untested edge case, not the golden path.
//
// Must run after ElementBindings (globalThis.Element, unwrap_element).
struct TemplateBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
