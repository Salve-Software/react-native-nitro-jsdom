#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "HybridHtmlSandboxSpec.hpp"
#include "LexborDocument.hpp"
#include "QuickJSRuntime.hpp"

namespace margelo::nitro::nitrojsdom {

class HybridHtmlSandbox : public HybridHtmlSandboxSpec {
public:
  HybridHtmlSandbox() : HybridObject(TAG), HybridHtmlSandboxSpec() {}
  ~HybridHtmlSandbox() override = default;

  void initialize(const std::string& html, bool runScripts, const std::string& url) override;

  std::shared_ptr<Promise<std::string>> evaluate(const std::string& script) override;

  std::string serialize() override;

  std::variant<nitro::NullType, std::string> querySelector(const std::string& selector) override;
  std::vector<std::string> querySelectorAll(const std::string& selector) override;

  std::variant<nitro::NullType, std::string> getAttribute(const std::string& selector, const std::string& attr) override;
  void setAttribute(const std::string& selector, const std::string& attr, const std::string& value) override;

  std::variant<nitro::NullType, std::string> getTextContent(const std::string& selector) override;
  std::variant<nitro::NullType, std::string> getInnerHTML(const std::string& selector) override;
  void setInnerHTML(const std::string& selector, const std::string& html) override;

private:
  std::unique_ptr<LexborDocument> _document;
  std::unique_ptr<QuickJSRuntime> _runtime;
  bool _initialized { false };
};

} // namespace margelo::nitro::nitrojsdom
