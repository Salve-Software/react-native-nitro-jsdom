#pragma once

#include <memory>
#include <string>

#include "HybridHtmlSandboxSpec.hpp"
#include "lexbor/LexborDocument.hpp"
#include "quickjs/QuickJSRuntime.hpp"

namespace margelo::nitro::nitrojsdom {

class HybridHtmlSandbox : public HybridHtmlSandboxSpec {
public:
  HybridHtmlSandbox() : HybridObject(TAG), HybridHtmlSandboxSpec() {}
  ~HybridHtmlSandbox() override = default;

  void initialize(const std::string& html, bool runScripts, const std::string& url) override;

  std::shared_ptr<Promise<std::string>> evaluate(const std::string& script) override;

  std::string serialize() override;

private:
  std::unique_ptr<LexborDocument> _document;
  std::unique_ptr<QuickJSRuntime> _runtime;
  bool _initialized { false };
};

} // namespace margelo::nitro::nitrojsdom
