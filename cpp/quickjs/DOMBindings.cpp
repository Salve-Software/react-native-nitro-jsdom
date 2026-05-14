#include "DOMBindings.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "QuickJSRuntime.hpp"
#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

static JSValue js_querySelector(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  auto result = doc->querySelector(sel);
  JS_FreeCString(ctx, sel);
  return result ? JS_NewString(ctx, result->c_str()) : JS_NULL;
}

static JSValue js_querySelectorAll(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto results = doc->querySelectorAll(sel);
  JS_FreeCString(ctx, sel);
  JSValue arr = JS_NewArray(ctx);
  for (size_t i = 0; i < results.size(); i++) {
    JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), JS_NewString(ctx, results[i].c_str()));
  }
  return arr;
}

static JSValue js_getAttribute(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 2) return JS_NULL;
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel  = JS_ToCString(ctx, argv[0]);
  const char* attr = JS_ToCString(ctx, argv[1]);
  if (!sel || !attr) {
    if (sel)  JS_FreeCString(ctx, sel);
    if (attr) JS_FreeCString(ctx, attr);
    return JS_NULL;
  }
  auto result = doc->getAttribute(sel, attr);
  JS_FreeCString(ctx, sel);
  JS_FreeCString(ctx, attr);
  return result ? JS_NewString(ctx, result->c_str()) : JS_NULL;
}

static JSValue js_setAttribute(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 3) return JS_UNDEFINED;
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel  = JS_ToCString(ctx, argv[0]);
  const char* attr = JS_ToCString(ctx, argv[1]);
  const char* val  = JS_ToCString(ctx, argv[2]);
  if (sel && attr && val) doc->setAttribute(sel, attr, val);
  JS_FreeCString(ctx, sel);
  JS_FreeCString(ctx, attr);
  JS_FreeCString(ctx, val);
  return JS_UNDEFINED;
}

static JSValue js_getTextContent(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  auto result = doc->getTextContent(sel);
  JS_FreeCString(ctx, sel);
  return result ? JS_NewString(ctx, result->c_str()) : JS_NULL;
}

static JSValue js_getInnerHTML(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  auto result = doc->getInnerHTML(sel);
  JS_FreeCString(ctx, sel);
  return result ? JS_NewString(ctx, result->c_str()) : JS_NULL;
}

static JSValue js_setInnerHTML(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 2) return JS_UNDEFINED;
  auto* doc = static_cast<LexborDocument*>(JS_GetContextOpaque(ctx));
  const char* sel  = JS_ToCString(ctx, argv[0]);
  const char* html = JS_ToCString(ctx, argv[1]);
  if (sel && html) doc->setInnerHTML(sel, html);
  JS_FreeCString(ctx, sel);
  JS_FreeCString(ctx, html);
  return JS_UNDEFINED;
}

void DOMBindings::install(QuickJSRuntime* runtime, LexborDocument* document) {
  JSContext* ctx = static_cast<JSContext*>(runtime->context());
  JS_SetContextOpaque(ctx, document);

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, doc, "querySelector",    JS_NewCFunction(ctx, js_querySelector,    "querySelector",    1));
  JS_SetPropertyStr(ctx, doc, "querySelectorAll", JS_NewCFunction(ctx, js_querySelectorAll, "querySelectorAll", 1));
  JS_SetPropertyStr(ctx, doc, "getAttribute",     JS_NewCFunction(ctx, js_getAttribute,     "getAttribute",     2));
  JS_SetPropertyStr(ctx, doc, "setAttribute",     JS_NewCFunction(ctx, js_setAttribute,     "setAttribute",     3));
  JS_SetPropertyStr(ctx, doc, "getTextContent",   JS_NewCFunction(ctx, js_getTextContent,   "getTextContent",   1));
  JS_SetPropertyStr(ctx, doc, "getInnerHTML",     JS_NewCFunction(ctx, js_getInnerHTML,     "getInnerHTML",     1));
  JS_SetPropertyStr(ctx, doc, "setInnerHTML",     JS_NewCFunction(ctx, js_setInnerHTML,     "setInnerHTML",     2));

  JS_SetPropertyStr(ctx, global, "document", doc);
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
