#include "DocumentBindings.hpp"
#include "LiveCollectionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

JSValue js_doc_getElementById(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* id = JS_ToCString(ctx, argv[0]);
  if (!id) return JS_NULL;
  void* el = get_doc(ctx)->getElementById(id);
  JS_FreeCString(ctx, id);
  return make_element(ctx, el);
}

JSValue js_doc_querySelector(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  void* el = get_doc(ctx)->querySelector_el(sel);
  JS_FreeCString(ctx, sel);
  return make_element(ctx, el);
}

JSValue js_doc_querySelectorAll(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto results = get_doc(ctx)->querySelectorAll_el(sel);
  JS_FreeCString(ctx, sel);
  return make_element_array(ctx, results);
}

JSValue js_doc_getElementsByClassName(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* names = JS_ToCString(ctx, argv[0]);
  if (!names) return JS_NewArray(ctx);
  JSValue result = LiveCollectionBindings::makeBySelector(ctx, nullptr, classNames_to_selector(names));
  JS_FreeCString(ctx, names);
  return result;
}

JSValue js_doc_getElementsByTagName(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NewArray(ctx);
  JSValue result = LiveCollectionBindings::makeBySelector(ctx, nullptr, tag);
  JS_FreeCString(ctx, tag);
  return result;
}

JSValue js_doc_createElement(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NULL;
  void* el = get_doc(ctx)->createElement(tag);
  JS_FreeCString(ctx, tag);
  return make_element(ctx, el);
}

JSValue js_doc_createTextNode(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* text = JS_ToCString(ctx, argv[0]);
  if (!text) return JS_NULL;
  void* node = get_doc(ctx)->createTextNode(text);
  JS_FreeCString(ctx, text);
  // Text nodes share the Element class so they can be passed to appendChild.
  return make_element(ctx, node);
}

JSValue js_doc_createComment(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NULL;
  const char* text = JS_ToCString(ctx, argv[0]);
  if (!text) return JS_NULL;
  void* node = get_doc(ctx)->createComment(text);
  JS_FreeCString(ctx, text);
  return make_element(ctx, node);
}

JSValue js_doc_createDocumentFragment(JSContext* ctx, JSValue, int, JSValue*) {
  return make_element(ctx, get_doc(ctx)->createDocumentFragment());
}

JSValue js_doc_get_body(JSContext* ctx, JSValue) {
  return make_element(ctx, get_doc(ctx)->body());
}

JSValue js_doc_get_head(JSContext* ctx, JSValue) {
  return make_element(ctx, get_doc(ctx)->head());
}

JSValue js_doc_get_documentElement(JSContext* ctx, JSValue) {
  return make_element(ctx, get_doc(ctx)->documentElement());
}

const char* kDocumentTitleBootstrapScript = R"JS(
(function() {
  Object.defineProperty(document, 'title', {
    get: function() {
      var head = document.head;
      if (!head) return '';
      var titleEl = head.querySelector('title');
      return titleEl ? titleEl.textContent : '';
    },
    set: function(value) {
      var head = document.head;
      if (!head) return;
      var titleEl = head.querySelector('title');
      if (!titleEl) {
        titleEl = document.createElement('title');
        head.appendChild(titleEl);
      }
      titleEl.textContent = String(value);
    },
    enumerable: true,
    configurable: true,
  });
})();
)JS";

} // namespace

void DocumentBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue doc = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, doc, "getElementById",         JS_NewCFunction(ctx, js_doc_getElementById,         "getElementById",         1));
  JS_SetPropertyStr(ctx, doc, "querySelector",          JS_NewCFunction(ctx, js_doc_querySelector,          "querySelector",          1));
  JS_SetPropertyStr(ctx, doc, "querySelectorAll",       JS_NewCFunction(ctx, js_doc_querySelectorAll,       "querySelectorAll",       1));
  JS_SetPropertyStr(ctx, doc, "getElementsByClassName", JS_NewCFunction(ctx, js_doc_getElementsByClassName, "getElementsByClassName", 1));
  JS_SetPropertyStr(ctx, doc, "getElementsByTagName",   JS_NewCFunction(ctx, js_doc_getElementsByTagName,   "getElementsByTagName",   1));
  JS_SetPropertyStr(ctx, doc, "createElement",          JS_NewCFunction(ctx, js_doc_createElement,          "createElement",          1));
  JS_SetPropertyStr(ctx, doc, "createTextNode",         JS_NewCFunction(ctx, js_doc_createTextNode,         "createTextNode",         1));
  JS_SetPropertyStr(ctx, doc, "createComment",          JS_NewCFunction(ctx, js_doc_createComment,          "createComment",          1));
  JS_SetPropertyStr(ctx, doc, "createDocumentFragment", JS_NewCFunction(ctx, js_doc_createDocumentFragment, "createDocumentFragment", 0));

  define_prop(ctx, doc, "body",            js_doc_get_body,            nullptr);
  define_prop(ctx, doc, "head",            js_doc_get_head,            nullptr);
  define_prop(ctx, doc, "documentElement", js_doc_get_documentElement, nullptr);

  RuntimeContext* rctx = get_ctx(ctx);
  bool hidden = !(rctx && rctx->pretend_to_be_visual);
  JS_SetPropertyStr(ctx, doc, "hidden", JS_NewBool(ctx, hidden));

  JSValue document_proto = JS_NewObject(ctx);
  JS_SetPrototype(ctx, doc, document_proto);
  JS_FreeValue(ctx, define_global_constructor(ctx, "Document", document_proto));
  JS_FreeValue(ctx, document_proto);

  JS_SetPropertyStr(ctx, global, "document", doc);
  JS_FreeValue(ctx, global);

  JSValue title_result = JS_Eval(ctx, kDocumentTitleBootstrapScript, strlen(kDocumentTitleBootstrapScript),
                                  "<document-title-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(title_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, title_result);
}

} // namespace margelo::nitro::nitrojsdom
