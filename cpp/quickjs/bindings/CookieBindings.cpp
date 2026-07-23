#include "CookieBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../Storage.hpp"
#include <string>

namespace margelo::nitro::nitrojsdom {

namespace {

std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
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
    size_t eq = pair.find('=');
    if (eq != std::string::npos) {
      std::string name = trim(pair.substr(0, eq));
      std::string value = trim(pair.substr(eq + 1));
      if (!name.empty()) rctx->cookie_jar.setItem(name, value);
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
