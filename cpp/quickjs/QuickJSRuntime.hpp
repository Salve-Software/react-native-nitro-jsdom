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

  ~RuntimeContext() = default;
};

// ── QuickJSRuntime ─────────────────────────────────────────────────────────────

class QuickJSRuntime {
public:
  QuickJSRuntime();
  ~QuickJSRuntime();

  void initialize(const std::string& url);
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
