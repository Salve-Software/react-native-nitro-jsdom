#include "HybridHtmlSandbox.hpp"
#include <stdexcept>
#include <variant>
#include <optional>
#include <functional>
#include <vector>
#include <string>

namespace margelo::nitro::nitrojsdom {

void HybridHtmlSandbox::initialize(const std::string& html, bool runScripts, const std::string& url) {
  _document = std::make_unique<LexborDocument>();
  _document->parse(html);

  _runtime = std::make_unique<QuickJSRuntime>();
  _runtime->initialize(url);
  _runtime->bindDocument(_document.get());

  if (runScripts) {
    for (const auto& script : _document->getScriptContents()) {
      _runtime->evaluate(script);
    }
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

void HybridHtmlSandbox::setConsoleCallback(
    const std::optional<std::variant<nitro::NullType, std::function<void(const std::string& level, const std::vector<std::string>& args)>>>& callback) {
  if (!_runtime) return;

  if (!callback.has_value()) {
    // No argument provided — clear the callback
    _runtime->setConsoleCallback(nullptr);
    return;
  }

  const auto& variant = callback.value();
  if (std::holds_alternative<nitro::NullType>(variant)) {
    // Explicit null passed — clear the callback
    _runtime->setConsoleCallback(nullptr);
  } else {
    // A real function was provided
    auto fn = std::get<std::function<void(const std::string&, const std::vector<std::string>&)>>(variant);
    _runtime->setConsoleCallback(
      [fn](std::string level, std::vector<std::string> args) {
        fn(level, args);
      }
    );
  }
}

void HybridHtmlSandbox::setDialogCallbacks(
    const std::optional<std::variant<nitro::NullType, std::function<void(const std::string& message)>>>& onAlert,
    const std::optional<std::variant<nitro::NullType, std::function<bool(const std::string& message)>>>& onConfirm,
    const std::optional<std::variant<nitro::NullType, std::function<std::variant<nitro::NullType, std::string>(const std::string& message, const std::optional<std::string>& defaultValue)>>>& onPrompt) {
  if (!_runtime) return;

  // ── onAlert ──────────────────────────────────────────────────────────────────
  if (!onAlert.has_value() || std::holds_alternative<nitro::NullType>(onAlert.value())) {
    _runtime->setAlertCallback(nullptr);
  } else {
    auto fn = std::get<std::function<void(const std::string&)>>(onAlert.value());
    _runtime->setAlertCallback([fn](const std::string& message) {
      fn(message);
    });
  }

  // ── onConfirm ────────────────────────────────────────────────────────────────
  if (!onConfirm.has_value() || std::holds_alternative<nitro::NullType>(onConfirm.value())) {
    _runtime->setConfirmCallback(nullptr);
  } else {
    auto fn = std::get<std::function<bool(const std::string&)>>(onConfirm.value());
    _runtime->setConfirmCallback([fn](const std::string& message) -> bool {
      return fn(message);
    });
  }

  // ── onPrompt ─────────────────────────────────────────────────────────────────
  if (!onPrompt.has_value() || std::holds_alternative<nitro::NullType>(onPrompt.value())) {
    _runtime->setPromptCallback(nullptr);
  } else {
    auto fn = std::get<std::function<std::variant<nitro::NullType, std::string>(const std::string&, const std::optional<std::string>&)>>(onPrompt.value());
    _runtime->setPromptCallback([fn](const std::string& message, const std::optional<std::string>& defaultValue) -> std::optional<std::string> {
      auto result = fn(message, defaultValue);
      if (std::holds_alternative<nitro::NullType>(result)) {
        return std::nullopt;
      }
      return std::get<std::string>(result);
    });
  }
}

} // namespace margelo::nitro::nitrojsdom
