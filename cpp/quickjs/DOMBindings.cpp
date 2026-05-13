#include "DOMBindings.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "QuickJSRuntime.hpp"

// TODO: include QuickJS headers once third_party/quickjs is added:
// #include <quickjs.h>

namespace margelo::nitro::nitrojsdom {

// TODO: When QuickJS is linked, implement each binding function below and
// register them on the global `document` object inside the JSContext.
//
// Example pattern for document.querySelector:
//
// static JSValue js_querySelector(JSContext* ctx, JSValue /*this_val*/,
//                                  int argc, JSValue* argv) {
//   if (argc < 1) return JS_UNDEFINED;
//   auto* doc = (LexborDocument*)JS_GetContextOpaque(ctx);
//   const char* selector = JS_ToCString(ctx, argv[0]);
//   auto result = doc->querySelector(selector);
//   JS_FreeCString(ctx, selector);
//   if (!result) return JS_NULL;
//   return JS_NewString(ctx, result->c_str());
// }
//
// static JSValue js_querySelectorAll(JSContext* ctx, JSValue /*this_val*/,
//                                     int argc, JSValue* argv) { ... }
// static JSValue js_getAttribute(...)  { ... }
// static JSValue js_setAttribute(...)  { ... }
// static JSValue js_getTextContent(...){ ... }
// static JSValue js_getInnerHTML(...)  { ... }
// static JSValue js_setInnerHTML(...)  { ... }

void DOMBindings::install(QuickJSRuntime* /*runtime*/, LexborDocument* /*document*/) {
  // TODO: Obtain JSContext* from runtime, set its opaque to `document`, then:
  //
  // JSContext* ctx = ...; // expose via QuickJSRuntime::context()
  // JS_SetContextOpaque(ctx, document);
  //
  // JSValue global = JS_GetGlobalObject(ctx);
  // JSValue doc = JS_NewObject(ctx);
  //
  // JS_SetPropertyStr(ctx, doc, "querySelector",
  //     JS_NewCFunction(ctx, js_querySelector, "querySelector", 1));
  // JS_SetPropertyStr(ctx, doc, "querySelectorAll",
  //     JS_NewCFunction(ctx, js_querySelectorAll, "querySelectorAll", 1));
  // JS_SetPropertyStr(ctx, doc, "getAttribute",
  //     JS_NewCFunction(ctx, js_getAttribute, "getAttribute", 2));
  // JS_SetPropertyStr(ctx, doc, "setAttribute",
  //     JS_NewCFunction(ctx, js_setAttribute, "setAttribute", 3));
  // JS_SetPropertyStr(ctx, doc, "textContent", ...);
  // JS_SetPropertyStr(ctx, doc, "innerHTML",   ...);
  //
  // JS_SetPropertyStr(ctx, global, "document", doc);
  // JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
