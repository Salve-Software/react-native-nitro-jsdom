#include "DOMParserBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <cstring>
#include <string>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_parsed_document_class_id = 0;
JSClassDef js_parsed_document_class = { "Document", .finalizer = nullptr };

LexborDocument* unwrap_parsed_doc(JSContext* ctx, JSValue val) {
  return static_cast<LexborDocument*>(JS_GetOpaque(val, js_parsed_document_class_id));
}

JSValue make_parsed_document(JSContext* ctx, LexborDocument* doc) {
  register_document(ctx, doc);
  JSValue obj = JS_NewObjectClass(ctx, js_parsed_document_class_id);
  JS_SetOpaque(obj, doc);
  register_document_wrapper(ctx, doc, obj);
  return obj;
}

// ── Document-scoped query/creation methods (mirror DocumentBindings.cpp's
// js_doc_* functions, but resolve `this`'s own LexborDocument instead of
// always reading the sandbox's primary get_doc(ctx)) ────────────────────────

JSValue js_pdoc_getElementById(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NULL;
  const char* id = JS_ToCString(ctx, argv[0]);
  if (!id) return JS_NULL;
  void* el = doc->getElementById(id);
  JS_FreeCString(ctx, id);
  return make_element(ctx, el);
}

JSValue js_pdoc_querySelector(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NULL;
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NULL;
  void* el = doc->querySelector_el(sel);
  JS_FreeCString(ctx, sel);
  return make_element(ctx, el);
}

JSValue js_pdoc_querySelectorAll(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NewArray(ctx);
  const char* sel = JS_ToCString(ctx, argv[0]);
  if (!sel) return JS_NewArray(ctx);
  auto results = doc->querySelectorAll_el(sel);
  JS_FreeCString(ctx, sel);
  return make_element_array(ctx, results);
}

// Static snapshot, not a live HTMLCollection — see header comment.
JSValue js_pdoc_getElementsByClassName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NewArray(ctx);
  const char* names = JS_ToCString(ctx, argv[0]);
  if (!names) return JS_NewArray(ctx);
  auto results = doc->querySelectorAll_el(classNames_to_selector(names));
  JS_FreeCString(ctx, names);
  return make_element_array(ctx, results);
}

// Static snapshot, not a live HTMLCollection — see header comment.
JSValue js_pdoc_getElementsByTagName(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NewArray(ctx);
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NewArray(ctx);
  auto results = doc->querySelectorAll_el(tag);
  JS_FreeCString(ctx, tag);
  return make_element_array(ctx, results);
}

JSValue js_pdoc_createElement(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NULL;
  const char* tag = JS_ToCString(ctx, argv[0]);
  if (!tag) return JS_NULL;
  void* el = doc->createElement(tag);
  JS_FreeCString(ctx, tag);
  return make_element(ctx, el);
}

JSValue js_pdoc_createTextNode(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NULL;
  const char* text = JS_ToCString(ctx, argv[0]);
  if (!text) return JS_NULL;
  void* node = doc->createTextNode(text);
  JS_FreeCString(ctx, text);
  return make_element(ctx, node);
}

JSValue js_pdoc_createComment(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc || argc < 1) return JS_NULL;
  const char* text = JS_ToCString(ctx, argv[0]);
  if (!text) return JS_NULL;
  void* node = doc->createComment(text);
  JS_FreeCString(ctx, text);
  return make_element(ctx, node);
}

JSValue js_pdoc_createDocumentFragment(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc) return JS_NULL;
  return make_element(ctx, doc->createDocumentFragment());
}

JSValue js_pdoc_get_body(JSContext* ctx, JSValue this_val) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  return doc ? make_element(ctx, doc->body()) : JS_NULL;
}

JSValue js_pdoc_get_head(JSContext* ctx, JSValue this_val) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  return doc ? make_element(ctx, doc->head()) : JS_NULL;
}

JSValue js_pdoc_get_documentElement(JSContext* ctx, JSValue this_val) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  return doc ? make_element(ctx, doc->documentElement()) : JS_NULL;
}

// Unlike document.doctype (DocumentBindings.cpp), this isn't cached: a
// secondary document is far less likely to have `.doctype` read repeatedly,
// so a fresh plain object per access keeps this file simpler.
JSValue js_pdoc_get_doctype(JSContext* ctx, JSValue this_val) {
  auto* doc = unwrap_parsed_doc(ctx, this_val);
  if (!doc) return JS_NULL;
  void* dt = doc->doctype();
  if (!dt) return JS_NULL;
  return build_doctype_object(ctx, doc, dt);
}

// ── DOMParser.parseFromString() / document.implementation.createHTMLDocument() ─

JSValue js_native_parse_from_string(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 2) return JS_ThrowTypeError(ctx, "parseFromString requires 2 arguments");
  const char* html = JS_ToCString(ctx, argv[0]);
  if (!html) return JS_EXCEPTION;
  const char* type = JS_ToCString(ctx, argv[1]);
  if (!type) { JS_FreeCString(ctx, html); return JS_EXCEPTION; }

  static const char* kSupportedTypes[] = {
    "text/html", "text/xml", "application/xml", "application/xhtml+xml", "image/svg+xml",
  };
  bool supported = false;
  for (const char* t : kSupportedTypes) {
    if (std::strcmp(type, t) == 0) { supported = true; break; }
  }
  if (!supported) {
    std::string msg = "Invalid DOMParser type: " + std::string(type);
    JS_FreeCString(ctx, html);
    JS_FreeCString(ctx, type);
    return JS_ThrowTypeError(ctx, "%s", msg.c_str());
  }
  // Every supported type is parsed with the HTML parser regardless of MIME
  // type — this sandbox has no separate XML parser wired up. Good enough for
  // the common case (parsing HTML/XHTML/SVG markup); genuine XML-specific
  // syntax (e.g. CDATA sections, processing instructions) is not modeled.
  JS_FreeCString(ctx, type);

  auto* doc = new LexborDocument();
  doc->parse(html);
  JS_FreeCString(ctx, html);

  auto* rctx = get_ctx(ctx);
  JSValue result = make_parsed_document(ctx, doc);
  if (rctx) rctx->extra_documents.emplace_back(doc);
  return result;
}

JSValue js_native_create_html_document(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  std::string title;
  if (argc >= 1 && !JS_IsUndefined(argv[0])) {
    const char* t = JS_ToCString(ctx, argv[0]);
    if (t) { title = t; JS_FreeCString(ctx, t); }
  }
  std::string escaped;
  escaped.reserve(title.size());
  for (char c : title) {
    if (c == '<') escaped += "&lt;";
    else if (c == '>') escaped += "&gt;";
    else if (c == '&') escaped += "&amp;";
    else escaped += c;
  }

  auto* doc = new LexborDocument();
  doc->parse("<!doctype html><html><head><title>" + escaped + "</title></head><body></body></html>");

  auto* rctx = get_ctx(ctx);
  JSValue result = make_parsed_document(ctx, doc);
  if (rctx) rctx->extra_documents.emplace_back(doc);
  return result;
}

const char* kDOMParserBootstrapScript = R"JS(
(function() {
  function DOMParser() {}
  DOMParser.prototype.parseFromString = function(str, type) {
    return __nativeParseFromString(String(str), String(type));
  };
  globalThis.DOMParser = DOMParser;

  document.implementation = {
    createHTMLDocument: function(title) {
      return __nativeCreateHTMLDocument(title === undefined ? undefined : String(title));
    },
  };

  function definePDocTitle(proto) {
    Object.defineProperty(proto, 'title', {
      configurable: true,
      get: function() {
        var head = this.head;
        if (!head) return '';
        var titleEl = head.querySelector('title');
        return titleEl ? titleEl.textContent : '';
      },
      set: function(value) {
        var head = this.head;
        if (!head) return;
        var titleEl = head.querySelector('title');
        if (!titleEl) {
          titleEl = this.createElement('title');
          head.appendChild(titleEl);
        }
        titleEl.textContent = String(value);
      },
    });
  }
  definePDocTitle(globalThis.__parsedDocumentProto);
  delete globalThis.__parsedDocumentProto;
})();
)JS";

} // namespace

void DOMParserBindings::install(JSContext* ctx) {
  JS_NewClassID(&js_parsed_document_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_parsed_document_class_id, &js_parsed_document_class);

  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, proto, "getElementById", JS_NewCFunction(ctx, js_pdoc_getElementById, "getElementById", 1));
  JS_SetPropertyStr(ctx, proto, "querySelector", JS_NewCFunction(ctx, js_pdoc_querySelector, "querySelector", 1));
  JS_SetPropertyStr(ctx, proto, "querySelectorAll", JS_NewCFunction(ctx, js_pdoc_querySelectorAll, "querySelectorAll", 1));
  JS_SetPropertyStr(ctx, proto, "getElementsByClassName", JS_NewCFunction(ctx, js_pdoc_getElementsByClassName, "getElementsByClassName", 1));
  JS_SetPropertyStr(ctx, proto, "getElementsByTagName", JS_NewCFunction(ctx, js_pdoc_getElementsByTagName, "getElementsByTagName", 1));
  JS_SetPropertyStr(ctx, proto, "createElement", JS_NewCFunction(ctx, js_pdoc_createElement, "createElement", 1));
  JS_SetPropertyStr(ctx, proto, "createTextNode", JS_NewCFunction(ctx, js_pdoc_createTextNode, "createTextNode", 1));
  JS_SetPropertyStr(ctx, proto, "createComment", JS_NewCFunction(ctx, js_pdoc_createComment, "createComment", 1));
  JS_SetPropertyStr(ctx, proto, "createDocumentFragment", JS_NewCFunction(ctx, js_pdoc_createDocumentFragment, "createDocumentFragment", 0));

  define_prop(ctx, proto, "body", js_pdoc_get_body, nullptr);
  define_prop(ctx, proto, "head", js_pdoc_get_head, nullptr);
  define_prop(ctx, proto, "documentElement", js_pdoc_get_documentElement, nullptr);
  define_prop(ctx, proto, "doctype", js_pdoc_get_doctype, nullptr);

  JS_SetPropertyStr(ctx, proto, "nodeType", JS_NewInt32(ctx, 9 /* DOCUMENT_NODE */));
  JS_SetPropertyStr(ctx, proto, "nodeName", JS_NewString(ctx, "#document"));
  JS_SetPropertyStr(ctx, proto, "ownerDocument", JS_NULL);
  define_node_type_constants(ctx, proto);

  JSValue global = JS_GetGlobalObject(ctx);

  // Chain onto Document.prototype (set up by DocumentBindings.cpp) purely so
  // `parsedDoc instanceof Document` holds, matching real jsdom/browsers.
  JSValue document_ctor = JS_GetPropertyStr(ctx, global, "Document");
  JSValue document_proto_val = JS_GetPropertyStr(ctx, document_ctor, "prototype");
  JS_SetPrototype(ctx, proto, document_proto_val);
  JS_FreeValue(ctx, document_proto_val);
  JS_FreeValue(ctx, document_ctor);

  JS_SetClassProto(ctx, js_parsed_document_class_id, JS_DupValue(ctx, proto));

  JS_SetPropertyStr(ctx, global, "__parsedDocumentProto", proto);
  JS_SetPropertyStr(ctx, global, "__nativeParseFromString", JS_NewCFunction(ctx, js_native_parse_from_string, "__nativeParseFromString", 2));
  JS_SetPropertyStr(ctx, global, "__nativeCreateHTMLDocument", JS_NewCFunction(ctx, js_native_create_html_document, "__nativeCreateHTMLDocument", 1));
  JS_FreeValue(ctx, global);

  JSValue result = JS_Eval(ctx, kDOMParserBootstrapScript, strlen(kDOMParserBootstrapScript),
                            "<domparser-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
