#include "HybridHtmlSandbox.hpp"
#include <stdexcept>
#include <variant>
#include <optional>
#include <functional>
#include <vector>
#include <string>
#include <cstdio>

namespace margelo::nitro::nitrojsdom {

namespace {
std::string jsonQuote(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += "\"";
  return out;
}
} // namespace

void HybridHtmlSandbox::initialize(const std::string& html, bool runScripts, const std::string& url, bool pretendToBeVisual) {
  _document = std::make_unique<LexborDocument>();
  _document->parse(html);

  _runtime = std::make_unique<QuickJSRuntime>();
  _runtime->initialize(url, pretendToBeVisual);
  _runtime->bindDocument(_document.get());

  if (runScripts) {
    for (const auto& script : _document->getScriptContents()) {
      try {
        _runtime->evaluate(script);
      } catch (...) { }
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
    const std::optional<std::variant<nitro::NullType, std::function<void(const std::string&)>>>& onAlert,
    const std::optional<std::variant<nitro::NullType, std::function<std::shared_ptr<Promise<bool>>(const std::string&)>>>& onConfirm,
    const std::optional<std::variant<nitro::NullType, std::function<std::shared_ptr<Promise<std::variant<nitro::NullType, std::string>>>(const std::string&, const std::optional<std::string>&)>>>& onPrompt) {
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
    auto fn = std::get<std::function<std::shared_ptr<Promise<bool>>(const std::string&)>>(onConfirm.value());
    _runtime->setConfirmCallback([fn](const std::string& message) -> bool {
      try {
        return fn(message)->await().get();
      } catch (...) {
        return false;
      }
    });
  }

  // ── onPrompt ─────────────────────────────────────────────────────────────────
  if (!onPrompt.has_value() || std::holds_alternative<nitro::NullType>(onPrompt.value())) {
    _runtime->setPromptCallback(nullptr);
  } else {
    using RetType = std::variant<nitro::NullType, std::string>;
    auto fn = std::get<std::function<std::shared_ptr<Promise<RetType>>(const std::string&, const std::optional<std::string>&)>>(onPrompt.value());
    _runtime->setPromptCallback([fn](const std::string& message, const std::optional<std::string>& defaultValue) -> std::optional<std::string> {
      try {
        auto val = fn(message, defaultValue)->await().get();
        if (std::holds_alternative<std::string>(val)) {
          return std::get<std::string>(val);
        }
        return std::nullopt;
      } catch (...) {
        return std::nullopt;
      }
    });
  }
}

void HybridHtmlSandbox::setFetchCallback(
    const std::optional<std::variant<nitro::NullType, std::function<std::shared_ptr<Promise<std::shared_ptr<Promise<std::string>>>>(const std::string&, const std::string&, const std::string&, const std::optional<std::string>&)>>>& callback) {
  if (!_runtime) return;

  if (!callback.has_value() || std::holds_alternative<nitro::NullType>(callback.value())) {
    _runtime->setFetchCallback(nullptr);
    return;
  }

  auto fn = std::get<std::function<std::shared_ptr<Promise<std::shared_ptr<Promise<std::string>>>>(const std::string&, const std::string&, const std::string&, const std::optional<std::string>&)>>(callback.value());
  _runtime->setFetchCallback(
    [fn](const std::string& url, const std::string& method, const std::string& headersJson, const std::optional<std::string>& body) -> std::string {
      try {
        // Outer await(): dispatches the JS callback call itself onto the JS thread and
        // waits for it to return — resolves almost immediately with the inner Promise object.
        // Inner await(): waits for that inner Promise (the real async onFetch() call) to settle.
        return fn(url, method, headersJson, body)->await().get()->await().get();
      } catch (const std::exception& e) {
        return std::string("{\"error\":") + jsonQuote(e.what()) + "}";
      } catch (...) {
        return "{\"error\":\"fetch failed\"}";
      }
    }
  );
}

} // namespace margelo::nitro::nitrojsdom
