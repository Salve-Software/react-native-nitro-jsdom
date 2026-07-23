#include "CookieBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../Storage.hpp"
#include <string>
#include <cctype>
#include <cstdlib>
#include <ctime>

namespace margelo::nitro::nitrojsdom {

namespace {

std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// Best-effort: accepts the date formats real cookie strings actually use
// (RFC 1123, RFC 850, asctime) — not a full HTTP-date parser.
bool is_past_date(const std::string& value) {
  static const char* kFormats[] = {
    "%a, %d %b %Y %H:%M:%S", // RFC 1123: "Thu, 01 Jan 1970 00:00:00 GMT"
    "%A, %d-%b-%y %H:%M:%S", // RFC 850
    "%a %b %d %H:%M:%S %Y",  // asctime
  };
  for (const char* fmt : kFormats) {
    struct tm parsed{};
    if (strptime(value.c_str(), fmt, &parsed)) {
      return timegm(&parsed) <= time(nullptr);
    }
  }
  return false;
}

// A cookie write is a deletion when `max-age` is <= 0 or `expires` names a
// date in the past — the two idioms real-world scripts use to clear a
// cookie (most commonly `expires=Thu, 01 Jan 1970 00:00:00 GMT`).
bool is_deletion(const std::string& attrs) {
  size_t pos = 0;
  while (pos < attrs.size()) {
    size_t semi = attrs.find(';', pos);
    std::string attr = attrs.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
    pos = semi == std::string::npos ? attrs.size() : semi + 1;

    size_t eq = attr.find('=');
    if (eq == std::string::npos) continue;
    std::string key = to_lower(trim(attr.substr(0, eq)));
    std::string value = trim(attr.substr(eq + 1));

    if (key == "max-age") {
      if (std::atol(value.c_str()) <= 0) return true;
    } else if (key == "expires") {
      if (is_past_date(value)) return true;
    }
  }
  return false;
}

JSValue js_doc_get_cookie(JSContext* ctx, JSValue) {
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_NewString(ctx, "");

  std::string result;
  for (size_t i = 0; i < rctx->cookie_jar.length(); i++) {
    auto key = rctx->cookie_jar.key(i);
    if (!key) continue;
    auto value = rctx->cookie_jar.getItem(*key);
    if (!value) continue;
    if (!result.empty()) result += "; ";
    result += *key + "=" + *value;
  }
  return JS_NewStringLen(ctx, result.data(), result.size());
}

JSValue js_doc_set_cookie(JSContext* ctx, JSValue, JSValue val) {
  auto* rctx = get_ctx(ctx);
  const char* str = JS_ToCString(ctx, val);
  if (str && rctx) {
    std::string input(str);
    size_t semi = input.find(';');
    std::string pair = semi == std::string::npos ? input : input.substr(0, semi);
    std::string attrs = semi == std::string::npos ? "" : input.substr(semi + 1);
    size_t eq = pair.find('=');
    if (eq != std::string::npos) {
      std::string name = trim(pair.substr(0, eq));
      std::string value = trim(pair.substr(eq + 1));
      if (!name.empty()) {
        if (is_deletion(attrs)) {
          rctx->cookie_jar.removeItem(name);
        } else {
          rctx->cookie_jar.setItem(name, value);
        }
      }
    }
  }
  if (str) JS_FreeCString(ctx, str);
  return JS_UNDEFINED;
}

} // namespace

void CookieBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc = JS_GetPropertyStr(ctx, global, "document");
  define_prop(ctx, doc, "cookie", js_doc_get_cookie, js_doc_set_cookie);
  JS_FreeValue(ctx, doc);
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
