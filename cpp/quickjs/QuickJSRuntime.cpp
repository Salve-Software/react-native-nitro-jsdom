#include "QuickJSRuntime.hpp"
#include "DOMBindings.hpp"
#include "../lexbor/LexborDocument.hpp"
#include <quickjs.h>
#include <stdexcept>
#include <string>
#include <mutex>

namespace margelo::nitro::nitrojsdom {

QuickJSRuntime::QuickJSRuntime() = default;

QuickJSRuntime::~QuickJSRuntime() {
  if (_context) JS_FreeContext(reinterpret_cast<JSContext*>(_context));
  if (_runtime) JS_FreeRuntime(reinterpret_cast<JSRuntime*>(_runtime));
  _context = nullptr;
  _runtime = nullptr;
}

void QuickJSRuntime::initialize(const std::string& url) {
  JSRuntime* rt = JS_NewRuntime();
  JSContext* ctx = JS_NewContext(rt);
  _runtime = rt;
  _context = ctx;

  JSValue global = JS_GetGlobalObject(ctx);

  // window = globalThis
  JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

  // location.href
  JSValue location = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, location, "href", JS_NewString(ctx, url.c_str()));
  JS_SetPropertyStr(ctx, global, "location", location);
  JS_FreeValue(ctx, location);

  JS_FreeValue(ctx, global);
}

void QuickJSRuntime::bindDocument(LexborDocument* document) {
  _document = document;
  DOMBindings::install(this, document);
}

std::string QuickJSRuntime::evaluate(const std::string& script) {
  JSContext* ctx = reinterpret_cast<JSContext*>(_context);
  std::lock_guard<std::mutex> lock(_mutex);

  JSValue result = JS_Eval(ctx, script.data(), script.size(), "<eval>", JS_EVAL_TYPE_GLOBAL);

  if (JS_IsException(result)) {
    JSValue ex = JS_GetException(ctx);
    JSValue msg = JS_ToString(ctx, ex);
    const char* str = JS_ToCString(ctx, msg);
    std::string err(str ? str : "Unknown JS error");
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, msg);
    JS_FreeValue(ctx, ex);
    JS_FreeValue(ctx, result);
    throw std::runtime_error(err);
  }

  JSValue strVal = JS_ToString(ctx, result);
  const char* str = JS_ToCString(ctx, strVal);
  std::string out(str ? str : "");
  JS_FreeCString(ctx, str);
  JS_FreeValue(ctx, strVal);
  JS_FreeValue(ctx, result);
  return out;
}

} // namespace margelo::nitro::nitrojsdom
