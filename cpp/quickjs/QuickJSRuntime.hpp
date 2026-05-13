#pragma once

#include <string>
#include <mutex>

namespace margelo::nitro::nitrojsdom {

class LexborDocument;

typedef struct JSContext JSContext;

class QuickJSRuntime {
public:
  QuickJSRuntime();
  ~QuickJSRuntime();

  void initialize(const std::string& url);
  void bindDocument(LexborDocument* document);
  std::string evaluate(const std::string& script);

  JSContext* context() const { return reinterpret_cast<JSContext*>(_context); }

private:
  void* _runtime  { nullptr };
  void* _context  { nullptr };

  LexborDocument* _document { nullptr };
  std::mutex _mutex;
};

} // namespace margelo::nitro::nitrojsdom
