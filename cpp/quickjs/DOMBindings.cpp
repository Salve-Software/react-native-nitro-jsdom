#include "DOMBindings.hpp"
#include "DOMBindingsInternal.hpp"
#include "MutationObservers.hpp"
#include "Storage.hpp"
#include "QuickJSRuntime.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "bindings/DOMExceptionBindings.hpp"
#include "bindings/ElementBindings.hpp"
#include "bindings/DocumentBindings.hpp"
#include "bindings/EventBindings.hpp"
#include "bindings/TimerBindings.hpp"
#include "bindings/WindowBindings.hpp"
#include "bindings/FetchBindings.hpp"
#include "bindings/LiveCollectionBindings.hpp"
#include "bindings/UrlBindings.hpp"
#include "bindings/AbortBindings.hpp"
#include "bindings/TextEncodingBindings.hpp"
#include "bindings/FormBindings.hpp"
#include "bindings/BlobBindings.hpp"
#include "bindings/CSSOMBindings.hpp"
#include "bindings/ShadowRootBindings.hpp"
#include "bindings/CustomElementsBindings.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>

// DOMBindings::install() is the single orchestrator for every QuickJS binding
// module below. Each module owns one concern and exposes `install(JSContext*)`;
// this function just sequences them (some depend on an earlier one having
// already registered a class or global — see the comments in each header).

namespace margelo::nitro::nitrojsdom {

void DOMBindings::install(QuickJSRuntime* runtime, LexborDocument* document) {
  JSContext* ctx = static_cast<JSContext*>(runtime->context());
  (void)document;

  DOMExceptionBindings::install(ctx); // no dependencies; other modules throw DOMException instances
  LiveCollectionBindings::install(ctx);
  ElementBindings::install(ctx);   // registers the Element class + proto
  ShadowRootBindings::install(ctx); // needs Element's proto + js_node_class_id's proto
  DocumentBindings::install(ctx);  // creates globalThis.document
  CustomElementsBindings::install(ctx); // needs Element/ShadowRoot protos + globalThis.document
  CSSOMBindings::install(ctx);     // needs Element's proto + globalThis.document to exist
  EventBindings::install(ctx);     // needs Element's proto + globalThis.document to exist
  TimerBindings::install(ctx);
  WindowBindings::install(ctx);
  UrlBindings::install(ctx);
  AbortBindings::install(ctx);
  TextEncodingBindings::install(ctx);
  BlobBindings::install(ctx);      // uses TextEncoder/TextDecoder + btoa, so must run after both
  FetchBindings::install(ctx);     // XHR bootstrap uses `new Event(...)`, so must run after EventBindings
  FormBindings::install(ctx);      // uses globalThis.Element + globalThis.Event, so must run after both

  // ── localStorage / sessionStorage ──────────────────────────────────────────
  {
    RuntimeContext* rctx = get_ctx(ctx);
    if (rctx) {
      installStorage(ctx, "localStorage", &rctx->local_storage);
      installStorage(ctx, "sessionStorage", &rctx->session_storage);
    }
  }

  // ── MutationObserver ───────────────────────────────────────────────────────
  {
    RuntimeContext* rctx = get_ctx(ctx);
    if (rctx && rctx->mutation_observers && rctx->document) {
      // Pass the lxb_dom_document_t* root node as the doc_root for ancestry checks
      void* html_doc = rctx->document->documentHtmlPtr();
      void* doc_node = lxb_dom_interface_node(
          lxb_dom_interface_document(
              static_cast<lxb_html_document_t*>(html_doc)));
      rctx->mutation_observers->install(ctx, doc_node);
    }
  }
}

} // namespace margelo::nitro::nitrojsdom
