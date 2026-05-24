#include "HybridHtmlSandbox.hpp"
#include <stdexcept>

namespace margelo::nitro::nitrojsdom {

void HybridHtmlSandbox::initialize(const std::string& html, bool runScripts, const std::string& url) {
  _document = std::make_unique<LexborDocument>();
  _document->parse(html);

  _runtime = std::make_unique<QuickJSRuntime>();
  _runtime->initialize(url);
  _runtime->bindDocument(_document.get());

  if (runScripts) {
    // TODO: extract and evaluate all <script> tags from the parsed document.
  }

  _initialized = true;
}

std::shared_ptr<Promise<std::string>> HybridHtmlSandbox::evaluate(const std::string& script) {
  if (!_initialized) {
    return Promise<std::string>::rejected(
      std::make_exception_ptr(std::runtime_error("HtmlSandbox: call initialize() before evaluate()"))
    );
  }
  return Promise<std::string>::async([this, script]() {
    return _runtime->evaluate(script);
  });
}

std::string HybridHtmlSandbox::serialize() {
  if (!_initialized) return "";
  return _document->serialize();
}

} // namespace margelo::nitro::nitrojsdom
