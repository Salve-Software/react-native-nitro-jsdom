#pragma once

#include <string>
#include <mutex>

namespace margelo::nitro::nitrojsdom {

class LexborDocument;

class QuickJSRuntime {
public:
  QuickJSRuntime();
  ~QuickJSRuntime();

  void initialize(const std::string& url);
  void bindDocument(LexborDocument* document);
  std::string evaluate(const std::string& script);

  void* context() const { return _context; }

private:
  void* _runtime  { nullptr };
  void* _context  { nullptr };

  LexborDocument* _document { nullptr };
  std::mutex _mutex;
};

} // namespace margelo::nitro::nitrojsdom
