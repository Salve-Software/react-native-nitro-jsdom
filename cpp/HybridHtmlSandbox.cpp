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

std::variant<nitro::NullType, std::string> HybridHtmlSandbox::querySelector(const std::string& selector) {
  if (!_initialized) return nitro::NullType{};
  auto result = _document->querySelector(selector);
  if (!result) return nitro::NullType{};
  return *result;
}

std::vector<std::string> HybridHtmlSandbox::querySelectorAll(const std::string& selector) {
  if (!_initialized) return {};
  return _document->querySelectorAll(selector);
}

std::variant<nitro::NullType, std::string> HybridHtmlSandbox::getAttribute(const std::string& selector, const std::string& attr) {
  if (!_initialized) return nitro::NullType{};
  auto result = _document->getAttribute(selector, attr);
  if (!result) return nitro::NullType{};
  return *result;
}

void HybridHtmlSandbox::setAttribute(const std::string& selector, const std::string& attr, const std::string& value) {
  if (!_initialized) return;
  _document->setAttribute(selector, attr, value);
}

std::variant<nitro::NullType, std::string> HybridHtmlSandbox::getTextContent(const std::string& selector) {
  if (!_initialized) return nitro::NullType{};
  auto result = _document->getTextContent(selector);
  if (!result) return nitro::NullType{};
  return *result;
}

std::variant<nitro::NullType, std::string> HybridHtmlSandbox::getInnerHTML(const std::string& selector) {
  if (!_initialized) return nitro::NullType{};
  auto result = _document->getInnerHTML(selector);
  if (!result) return nitro::NullType{};
  return *result;
}

void HybridHtmlSandbox::setInnerHTML(const std::string& selector, const std::string& html) {
  if (!_initialized) return;
  _document->setInnerHTML(selector, html);
}

} // namespace margelo::nitro::nitrojsdom
