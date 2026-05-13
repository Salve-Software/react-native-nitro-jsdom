#pragma once

#include <string>

namespace margelo::nitro::nitrojsdom {

class LexborDocument;

/**
 * Wraps a QuickJS runtime + context.
 *
 * Each instance owns an isolated JS runtime — no shared state with other instances
 * or with the React Native Hermes runtime.
 *
 * Integration: add QuickJS sources (quickjs.c / quickjs.h) to third_party/quickjs
 * and link via CMakeLists.txt. Include <quickjs.h> and replace stub bodies.
 */
class QuickJSRuntime {
public:
  QuickJSRuntime();
  ~QuickJSRuntime();

  // Create the QuickJS runtime + context and set window.location.href.
  void initialize(const std::string& url);

  // Wire up document.* methods so they delegate to the given LexborDocument.
  void bindDocument(LexborDocument* document);

  // Evaluate a JS expression/statement and return the result as a string.
  // Throws std::runtime_error on JS exceptions.
  std::string evaluate(const std::string& script);

private:
  // Opaque pointers to JSRuntime* / JSContext* — will be real types once QuickJS is linked.
  void* _runtime  { nullptr };
  void* _context  { nullptr };

  LexborDocument* _document { nullptr };
};

} // namespace margelo::nitro::nitrojsdom
