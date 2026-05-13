#pragma once

namespace margelo::nitro::nitrojsdom {

class LexborDocument;
class QuickJSRuntime;

/**
 * Registers document.*, window.*, and console.* bindings inside a QuickJS context,
 * delegating DOM operations to the provided LexborDocument.
 *
 * Called once during QuickJSRuntime::bindDocument().
 */
class DOMBindings {
public:
  // Register all built-in JS ↔ Lexbor bindings on the given runtime/context pair.
  // `runtime` and `document` must outlive this call.
  static void install(QuickJSRuntime* runtime, LexborDocument* document);
};

} // namespace margelo::nitro::nitrojsdom
