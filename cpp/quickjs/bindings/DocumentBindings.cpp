#include "DocumentBindings.hpp"
#include "LiveCollectionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <cstring>
#include <string>

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

JSValue js_doc_getElementsByName(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewArray(ctx);
  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_NewArray(ctx);
  std::string selector = "[name=\"" + std::string(name) + "\"]";
  JS_FreeCString(ctx, name);
  return LiveCollectionBindings::makeBySelector(ctx, nullptr, selector);
}

// document.forms/.images/.scripts/.links — live HTMLCollections backed by the
// same LiveCollectionBindings::makeBySelector primitive that already backs
// getElementsByClassName/getElementsByTagName. .links matches jsdom: any <a>
// or <area> that actually has an href attribute (bare <a> anchors don't count).
JSValue js_doc_get_forms(JSContext* ctx, JSValue) {
  return LiveCollectionBindings::makeBySelector(ctx, nullptr, "form");
}

JSValue js_doc_get_images(JSContext* ctx, JSValue) {
  return LiveCollectionBindings::makeBySelector(ctx, nullptr, "img");
}

JSValue js_doc_get_scripts(JSContext* ctx, JSValue) {
  return LiveCollectionBindings::makeBySelector(ctx, nullptr, "script");
}

JSValue js_doc_get_links(JSContext* ctx, JSValue) {
  return LiveCollectionBindings::makeBySelector(ctx, nullptr, "a[href], area[href]");
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

// document.doctype is exposed as a plain {name, publicId, systemId, nodeType,
// nodeName} object rather than routed through make_element()/the generic
// Node wrapper: DocumentType's `name` is an interned lxb_dom_attr_id_t (needs
// LexborDocument::doctypeName(), not the generic nodeName getter), and it has
// no other Node behavior real-world embedded scripts rely on (no children,
// not appendChild-able). Since it's a plain object rather than a cached
// node-pointer-keyed wrapper, the constructed object is cached directly on
// RuntimeContext (built once, on first access) so repeated `document.doctype`
// reads return the same JS object — the document itself never gets
// re-parsed mid-instance, so the cache never needs invalidation.
JSValue js_doc_get_doctype(JSContext* ctx, JSValue) {
  void* dt = get_doc(ctx)->doctype();
  if (!dt) return JS_NULL;

  auto* rctx = get_ctx(ctx);
  if (rctx && rctx->doctype_wrapper) {
    return JS_DupValue(ctx, *static_cast<JSValue*>(rctx->doctype_wrapper));
  }

  JSValue obj = JS_NewObject(ctx);
  std::string name = get_doc(ctx)->doctypeName(dt);
  JS_SetPropertyStr(ctx, obj, "name",      JS_NewStringLen(ctx, name.data(), name.size()));
  std::string publicId = get_doc(ctx)->doctypePublicId(dt);
  JS_SetPropertyStr(ctx, obj, "publicId",  JS_NewStringLen(ctx, publicId.data(), publicId.size()));
  std::string systemId = get_doc(ctx)->doctypeSystemId(dt);
  JS_SetPropertyStr(ctx, obj, "systemId",  JS_NewStringLen(ctx, systemId.data(), systemId.size()));
  JS_SetPropertyStr(ctx, obj, "nodeType",  JS_NewInt32(ctx, 10));
  JS_SetPropertyStr(ctx, obj, "nodeName",  JS_NewStringLen(ctx, name.data(), name.size()));

  if (rctx) rctx->doctype_wrapper = new JSValue(JS_DupValue(ctx, obj));
  return obj;
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
  JS_SetPropertyStr(ctx, doc, "getElementsByName",      JS_NewCFunction(ctx, js_doc_getElementsByName,      "getElementsByName",      1));
  JS_SetPropertyStr(ctx, doc, "createElement",          JS_NewCFunction(ctx, js_doc_createElement,          "createElement",          1));
  JS_SetPropertyStr(ctx, doc, "createTextNode",         JS_NewCFunction(ctx, js_doc_createTextNode,         "createTextNode",         1));
  JS_SetPropertyStr(ctx, doc, "createComment",          JS_NewCFunction(ctx, js_doc_createComment,          "createComment",          1));
  JS_SetPropertyStr(ctx, doc, "createDocumentFragment", JS_NewCFunction(ctx, js_doc_createDocumentFragment, "createDocumentFragment", 0));

  define_prop(ctx, doc, "body",            js_doc_get_body,            nullptr);
  define_prop(ctx, doc, "head",            js_doc_get_head,            nullptr);
  define_prop(ctx, doc, "documentElement", js_doc_get_documentElement, nullptr);
  define_prop(ctx, doc, "doctype",         js_doc_get_doctype,         nullptr);
  define_prop(ctx, doc, "forms",           js_doc_get_forms,           nullptr);
  define_prop(ctx, doc, "images",          js_doc_get_images,          nullptr);
  define_prop(ctx, doc, "scripts",         js_doc_get_scripts,         nullptr);
  define_prop(ctx, doc, "links",           js_doc_get_links,           nullptr);

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
