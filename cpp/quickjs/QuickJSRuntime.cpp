#include "QuickJSRuntime.hpp"
#include "DOMBindings.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "quickjs.h"
#include <stdexcept>
#include <string>
#include <mutex>
#ifdef __ANDROID__
#include <android/log.h>
#define QJS_LOG(...) __android_log_print(ANDROID_LOG_ERROR, "NitroJsdom", __VA_ARGS__)
#else
#define QJS_LOG(...) ((void)0)
#endif

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
  if (!rt) throw std::runtime_error("QuickJS: failed to create runtime");
  JSContext* ctx = JS_NewContext(rt);
  if (!ctx) {
    JS_FreeRuntime(rt);
    throw std::runtime_error("QuickJS: failed to create context");
  }
  _runtime = rt;
  _context = ctx;

  JSValue global = JS_GetGlobalObject(ctx);

  // window = globalThis
  JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

  // location.href
  JSValue location = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, location, "href", JS_NewString(ctx, url.c_str()));
  JS_SetPropertyStr(ctx, global, "location", location);
  // location consumed by SetPropertyStr — no FreeValue here

  JS_FreeValue(ctx, global);
}

void QuickJSRuntime::bindDocument(LexborDocument* document) {
  if (!_context) return;
  _document = document;
  DOMBindings::install(this, document);
}

std::string QuickJSRuntime::evaluate(const std::string& script) {
  JSRuntime* rt  = reinterpret_cast<JSRuntime*>(_runtime);
  JSContext* ctx = reinterpret_cast<JSContext*>(_context);
  std::lock_guard<std::mutex> lock(_mutex);

  // Update the stack reference so QuickJS doesn't false-positive overflow
  // when evaluate() runs on a different thread than initialize().
  JS_UpdateStackTop(rt);

  JSValue result = JS_Eval(ctx, script.data(), script.size(), "<eval>", JS_EVAL_TYPE_GLOBAL);

  if (JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    JSValue ex = JS_GetException(ctx);
    std::string err = "Unknown JS error";

    // Try .message property first (Error objects)
    JSValue msg_prop = JS_GetPropertyStr(ctx, ex, "message");
    if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
      const char* s = JS_ToCString(ctx, msg_prop);
      if (s) { err = s; JS_FreeCString(ctx, s); }
    }
    JS_FreeValue(ctx, msg_prop);

    // Fallback: toString() the exception
    if (err == "Unknown JS error") {
      JSValue str_val = JS_ToString(ctx, ex);
      if (!JS_IsException(str_val)) {
        const char* s = JS_ToCString(ctx, str_val);
        if (s) { err = s; JS_FreeCString(ctx, s); }
      }
      JS_FreeValue(ctx, str_val);
    }

    JS_FreeValue(ctx, ex);
    QJS_LOG("QuickJS exception: %s", err.c_str());
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
