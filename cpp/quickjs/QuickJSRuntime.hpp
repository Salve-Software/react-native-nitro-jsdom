#pragma once

#include <string>
#include <mutex>
#include <functional>
#include <vector>
#include <memory>
#include <queue>
#include <unordered_map>
#include <optional>

#include "Storage.hpp"

namespace margelo::nitro::nitrojsdom {

class LexborDocument;
class MutationObservers;

// ── Timer ──────────────────────────────────────────────────────────────────────

struct Timer {
  uint32_t id;
  bool repeat;         // false = setTimeout, true = setInterval
  int64_t interval_ms;
  int64_t fire_at_ms;  // steady_clock epoch ms
  void* callback;      // JSValue* (heap-allocated, owned)
  bool cancelled { false };
};

struct TimerCmp {
  bool operator()(const Timer* a, const Timer* b) const {
    return a->fire_at_ms > b->fire_at_ms; // min-heap: smallest fire_at first
  }
};

// ── EventListener ──────────────────────────────────────────────────────────────

struct EventListener {
  void* node;              // lxb_dom_node_t*
  std::string event_type;
  void* callback;          // JSValue* (heap-allocated, owned)
  bool is_handler_property { false };
};

// ── RuntimeContext ─────────────────────────────────────────────────────────────

struct RuntimeContext {
  LexborDocument* document { nullptr };
  void* runtime { nullptr }; // QuickJSRuntime*

  uint32_t next_timer_id { 1 };
  std::priority_queue<Timer*, std::vector<Timer*>, TimerCmp> timer_heap;
  std::unordered_map<uint32_t, Timer*> timer_map;

  std::vector<EventListener> listeners;
  std::function<void(std::string level, std::vector<std::string> args)> console_callback;

  std::function<void(const std::string& message)> alert_callback;
  std::function<bool(const std::string& message)> confirm_callback;
  std::function<std::optional<std::string>(const std::string& message, const std::optional<std::string>& defaultValue)> prompt_callback;

  std::function<std::string(const std::string& url, const std::string& method, const std::string& headersJson, const std::optional<std::string>& body)> fetch_callback;

  void* pending_rejection { nullptr };
  std::unique_ptr<MutationObservers> mutation_observers;

  Storage local_storage;
  Storage session_storage;

  // In-memory document.cookie jar (name -> value). No real navigation/origin
  // model exists in this sandbox, so cookie attributes (expires/path/domain/
  // secure/samesite) are parsed off the setter's input and discarded rather
  // than enforced — see CookieBindings.
  Storage cookie_jar;

  bool pretend_to_be_visual { false };
  double time_origin_ms { 0 };

  void* active_element { nullptr };
  void* current_script { nullptr };
  std::string ready_state { "loading" };

  // node pointer → heap-allocated JSValue* (DupValue'd strong ref)
  // Ensures the same native node always returns the same JS wrapper object.
  std::unordered_map<void*, void*> node_wrapper_cache;

  // document.doctype's plain-object wrapper (heap-allocated JSValue*,
  // DupValue'd strong ref) — not routed through node_wrapper_cache since
  // it isn't keyed by a native node pointer (see DocumentBindings.cpp).
  // Built lazily on first access and reused after that, so repeated
  // `document.doctype` reads return the same JS object (identity-stable).
  void* doctype_wrapper { nullptr };

  // host element pointer (lxb_dom_element_t*) → its shadow root
  // (lxb_dom_shadow_root_t*). Lexbor's element struct has no built-in
  // back-pointer to an attached shadow root, so we track it ourselves.
  std::unordered_map<void*, void*> element_shadow_roots;

  // Secondary documents created via DOMParser.parseFromString() /
  // document.implementation.createHTMLDocument() (see DOMParserBindings).
  // Owned here rather than by their JS wrapper's finalizer: QuickJS class
  // finalizers only receive a JSRuntime*, not the JSContext needed to safely
  // touch document_registry below, so they're freed together with the rest
  // of the sandbox instead of individually via GC.
  std::vector<std::unique_ptr<LexborDocument>> extra_documents;

  // Raw lexbor document pointer (lxb_dom_document_t*) -> the LexborDocument
  // wrapper that owns it (primary sandbox document, or one of
  // extra_documents). Lets node-scoped bindings that create new nodes
  // (textContent/innerHTML setters, insertAdjacentHTML, matches()/closest())
  // resolve the *correct* owning document instead of assuming the sandbox's
  // primary `document` — which would otherwise create nodes in the wrong
  // document's memory arena when called on a DOMParser-produced element.
  // See doc_for_node() in DOMBindingsInternal.
  std::unordered_map<void*, LexborDocument*> document_registry;

  // LexborDocument* -> its JS wrapper (JSValue*, heap-allocated, owned).
  // Lets node.ownerDocument return the same JS object DOMParser.
  // parseFromString()/createHTMLDocument() already handed the caller,
  // instead of building a second, unequal wrapper for the same document.
  std::unordered_map<LexborDocument*, void*> document_wrappers;

  ~RuntimeContext() = default;
};

// ── QuickJSRuntime ─────────────────────────────────────────────────────────────

class QuickJSRuntime {
public:
  QuickJSRuntime();
  ~QuickJSRuntime();

  void initialize(const std::string& url, bool pretendToBeVisual);
  void bindDocument(LexborDocument* document);
  std::string evaluate(const std::string& script);

  void setConsoleCallback(std::function<void(std::string level, std::vector<std::string> args)> cb);

  void setAlertCallback(std::function<void(const std::string&)> cb);
  void setConfirmCallback(std::function<bool(const std::string&)> cb);
  void setPromptCallback(std::function<std::optional<std::string>(const std::string&, const std::optional<std::string>&)> cb);

  void setFetchCallback(std::function<std::string(const std::string&, const std::string&, const std::string&, const std::optional<std::string>&)> cb);

  void* context() const { return _context; }
  RuntimeContext* contextState() const { return _ctxState.get(); }

private:
  void drainEventLoop();
  void fireTimer(Timer* t);
  void cleanupTimers();

  void* _runtime  { nullptr };
  void* _context  { nullptr };

  std::unique_ptr<RuntimeContext> _ctxState;
  std::mutex _mutex;
};

} // namespace margelo::nitro::nitrojsdom
