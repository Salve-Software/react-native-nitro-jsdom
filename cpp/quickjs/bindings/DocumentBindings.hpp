#pragma once

// Creates globalThis.document and attaches its non-event methods/properties:
// getElementById, querySelector(All), getElementsBy{ClassName,TagName},
// createElement, createTextNode, body/head/documentElement, and document.title.
//
// EventBindings::install() runs afterwards and attaches
// addEventListener/dispatchEvent onto the same `document` object.

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class DocumentBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
