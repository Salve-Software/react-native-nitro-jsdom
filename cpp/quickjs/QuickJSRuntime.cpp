#include "QuickJSRuntime.hpp"
#include "DOMBindings.hpp"
#include "../lexbor/LexborDocument.hpp"

// TODO: include QuickJS headers once third_party/quickjs is added:
// #include <quickjs.h>

namespace margelo::nitro::nitrojsdom {

QuickJSRuntime::QuickJSRuntime() = default;

QuickJSRuntime::~QuickJSRuntime() {
  // TODO: JS_FreeContext((JSContext*)_context);
  //       JS_FreeRuntime((JSRuntime*)_runtime);
  _context = nullptr;
  _runtime = nullptr;
}

void QuickJSRuntime::initialize(const std::string& /*url*/) {
  // TODO: Real QuickJS setup:
  // _runtime = JS_NewRuntime();
  // _context = JS_NewContext((JSRuntime*)_runtime);
  //
  // Set window.location.href:
  // JSValue global = JS_GetGlobalObject((JSContext*)_context);
  // JSValue location = JS_NewObject((JSContext*)_context);
  // JS_SetPropertyStr((JSContext*)_context, location, "href",
  //     JS_NewString((JSContext*)_context, url.c_str()));
  // JS_SetPropertyStr((JSContext*)_context, global, "location", location);
  // JS_FreeValue((JSContext*)_context, global);
}

void QuickJSRuntime::bindDocument(LexborDocument* document) {
  _document = document;
  // TODO: DOMBindings::install(this, document);
}

std::string QuickJSRuntime::evaluate(const std::string& /*script*/) {
  // TODO: Real QuickJS evaluation:
  // JSValue result = JS_Eval((JSContext*)_context,
  //     script.c_str(), script.size(),
  //     "<eval>", JS_EVAL_TYPE_GLOBAL);
  //
  // if (JS_IsException(result)) {
  //   JSValue exception = JS_GetException((JSContext*)_context);
  //   const char* msg = JS_ToCString((JSContext*)_context, exception);
  //   std::string err(msg);
  //   JS_FreeCString((JSContext*)_context, msg);
  //   JS_FreeValue((JSContext*)_context, exception);
  //   JS_FreeValue((JSContext*)_context, result);
  //   throw std::runtime_error(err);
  // }
  //
  // JSValue strVal = JS_ToString((JSContext*)_context, result);
  // const char* str = JS_ToCString((JSContext*)_context, strVal);
  // std::string out(str);
  // JS_FreeCString((JSContext*)_context, str);
  // JS_FreeValue((JSContext*)_context, strVal);
  // JS_FreeValue((JSContext*)_context, result);
  // return out;

  return "";
}

} // namespace margelo::nitro::nitrojsdom
