#include "QuickJSRuntime.hpp"
#include "DOMBindings.hpp"
#include "MutationObservers.hpp"
#include "../lexbor/LexborDocument.hpp"
#include "quickjs.h"
#include <stdexcept>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#define QJS_LOG(...) __android_log_print(ANDROID_LOG_ERROR, "NitroJsdom", __VA_ARGS__)
#else
#define QJS_LOG(...) ((void)0)
#endif

namespace margelo::nitro::nitrojsdom {

// ── Helpers ────────────────────────────────────────────────────────────────────

static int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ── Promise rejection tracker ──────────────────────────────────────────────────

static void promise_rejection_tracker(JSContext* ctx, JSValue promise,
                                       JSValue reason, JS_BOOL is_handled,
                                       void* opaque) {
  (void)promise;
  RuntimeContext* rctx = static_cast<RuntimeContext*>(opaque);
  if (is_handled) {
    if (rctx->pending_rejection) {
      JSValue* stored = static_cast<JSValue*>(rctx->pending_rejection);
      JS_FreeValue(ctx, *stored);
      delete stored;
      rctx->pending_rejection = nullptr;
    }
    return;
  }
  // Free any previously stored rejection
  if (rctx->pending_rejection) {
    JSValue* stored = static_cast<JSValue*>(rctx->pending_rejection);
    JS_FreeValue(ctx, *stored);
    delete stored;
    rctx->pending_rejection = nullptr;
  }
  JSValue* copy = new JSValue(JS_DupValue(ctx, reason));
  rctx->pending_rejection = copy;
}

// ── Constructor / Destructor ───────────────────────────────────────────────────

QuickJSRuntime::QuickJSRuntime() = default;

QuickJSRuntime::~QuickJSRuntime() {
  // prevent races with drainEventLoop
  std::lock_guard<std::mutex> lock(_mutex);
  if (_context && _ctxState) {
    JSContext* ctx = reinterpret_cast<JSContext*>(_context);
    cleanupTimers();

    // Free all event listener callbacks
    for (auto& listener : _ctxState->listeners) {
      if (listener.callback) {
        JSValue* cb = static_cast<JSValue*>(listener.callback);
        JS_FreeValue(ctx, *cb);
        delete cb;
      }
    }
    _ctxState->listeners.clear();

    // Free pending rejection
    if (_ctxState->pending_rejection) {
      JSValue* stored = static_cast<JSValue*>(_ctxState->pending_rejection);
      JS_FreeValue(ctx, *stored);
      delete stored;
      _ctxState->pending_rejection = nullptr;
    }

    // Free MutationObserver callbacks — must happen before JS_FreeContext
    if (_ctxState->mutation_observers) {
      _ctxState->mutation_observers->clearAll(ctx);
    }
  }

  if (_context) JS_FreeContext(reinterpret_cast<JSContext*>(_context));
  if (_runtime) JS_FreeRuntime(reinterpret_cast<JSRuntime*>(_runtime));
  _context = nullptr;
  _runtime = nullptr;
}

void QuickJSRuntime::cleanupTimers() {
  if (!_context || !_ctxState) return;
  JSContext* ctx = reinterpret_cast<JSContext*>(_context);

  // drain the heap first — cancelled timers are erased from timer_map but kept
  // in the heap until popped, so the heap is the authoritative cleanup loop.
  while (!_ctxState->timer_heap.empty()) {
    Timer* t = _ctxState->timer_heap.top();
    _ctxState->timer_heap.pop();
    // Only free timers still owned by timer_map (not yet erased by clearTimer)
    if (_ctxState->timer_map.find(t->id) != _ctxState->timer_map.end()) {
      if (t->callback) {
        JSValue* cb = static_cast<JSValue*>(t->callback);
        JS_FreeValue(ctx, *cb);
        delete cb;
        t->callback = nullptr;
      }
      _ctxState->timer_map.erase(t->id);
      delete t;
    } else {
      // Already erased from map (cancelled) — just delete the dangling heap ptr
      delete t;
    }
  }
  // Any remaining map entries not in the heap (should not happen, but be safe)
  for (auto& [id, t] : _ctxState->timer_map) {
    if (t->callback) {
      JSValue* cb = static_cast<JSValue*>(t->callback);
      JS_FreeValue(ctx, *cb);
      delete cb;
      t->callback = nullptr;
    }
    delete t;
  }
  _ctxState->timer_map.clear();
}

// ── initialize ─────────────────────────────────────────────────────────────────

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

  // Create RuntimeContext and attach to QuickJS context opaque
  _ctxState = std::make_unique<RuntimeContext>();
  _ctxState->runtime = this;
  _ctxState->mutation_observers = std::make_unique<MutationObservers>();
  JS_SetContextOpaque(ctx, _ctxState.get());

  // Register promise rejection tracker
  JS_SetHostPromiseRejectionTracker(rt, promise_rejection_tracker, _ctxState.get());

  JSValue global = JS_GetGlobalObject(ctx);

  // window = globalThis
  JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

  // location.href
  JSValue location = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, location, "href", JS_NewString(ctx, url.c_str()));
  JS_SetPropertyStr(ctx, global, "location", location);

  JS_FreeValue(ctx, global);
}

// ── bindDocument ──────────────────────────────────────────────────────────────

void QuickJSRuntime::bindDocument(LexborDocument* document) {
  if (!_context) return;
  _ctxState->document = document;
  DOMBindings::install(this, document);
}

// ── setConsoleCallback ────────────────────────────────────────────────────────

void QuickJSRuntime::setConsoleCallback(
    std::function<void(std::string level, std::vector<std::string> args)> cb) {
  if (_ctxState) _ctxState->console_callback = std::move(cb);
}

// ── Dialog callbacks ──────────────────────────────────────────────────────────

void QuickJSRuntime::setAlertCallback(std::function<void(const std::string&)> cb) {
  if (_ctxState) _ctxState->alert_callback = std::move(cb);
}

void QuickJSRuntime::setConfirmCallback(std::function<bool(const std::string&)> cb) {
  if (_ctxState) _ctxState->confirm_callback = std::move(cb);
}

void QuickJSRuntime::setPromptCallback(
    std::function<std::optional<std::string>(const std::string&, const std::optional<std::string>&)> cb) {
  if (_ctxState) _ctxState->prompt_callback = std::move(cb);
}

// ── Event loop drain ──────────────────────────────────────────────────────────

void QuickJSRuntime::drainEventLoop() {
  JSRuntime* rt  = reinterpret_cast<JSRuntime*>(_runtime);
  JSContext* ctx = reinterpret_cast<JSContext*>(_context);

  while (true) {
    // 1. Drain all pending microtasks / Promise jobs
    JSContext* job_ctx = nullptr;
    while (true) {
      int r = JS_ExecutePendingJob(rt, &job_ctx);
      if (r == 0) break; // no more jobs
      if (r < 0) {
        // A job threw — capture exception
        JSValue ex = JS_GetException(job_ctx ? job_ctx : ctx);
        std::string err = "Unknown async error";
        JSValue msg_prop = JS_GetPropertyStr(ctx, ex, "message");
        if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
          const char* s = JS_ToCString(ctx, msg_prop);
          if (s) { err = s; JS_FreeCString(ctx, s); }
        }
        JS_FreeValue(ctx, msg_prop);
        if (err == "Unknown async error") {
          JSValue str_val = JS_ToString(ctx, ex);
          if (!JS_IsException(str_val)) {
            const char* s = JS_ToCString(ctx, str_val);
            if (s) { err = s; JS_FreeCString(ctx, s); }
          }
          JS_FreeValue(ctx, str_val);
        }
        JS_FreeValue(ctx, ex);
        throw std::runtime_error(err);
      }
    }

    // 2. Check for unhandled rejection set by the tracker
    if (_ctxState->pending_rejection) {
      JSValue* stored = static_cast<JSValue*>(_ctxState->pending_rejection);
      std::string err = "Unhandled Promise rejection";
      JSValue msg_prop = JS_GetPropertyStr(ctx, *stored, "message");
      if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
        const char* s = JS_ToCString(ctx, msg_prop);
        if (s) { err = s; JS_FreeCString(ctx, s); }
      }
      JS_FreeValue(ctx, msg_prop);
      if (err == "Unhandled Promise rejection") {
        JSValue str_val = JS_ToString(ctx, *stored);
        if (!JS_IsException(str_val)) {
          const char* s = JS_ToCString(ctx, str_val);
          if (s) { err = s; JS_FreeCString(ctx, s); }
        }
        JS_FreeValue(ctx, str_val);
      }
      JS_FreeValue(ctx, *stored);
      delete stored;
      _ctxState->pending_rejection = nullptr;
      throw std::runtime_error(err);
    }

    // 3. Fire due timers
    int64_t t_now = now_ms();
    bool fired_any = false;

    while (!_ctxState->timer_heap.empty()) {
      Timer* top = _ctxState->timer_heap.top();

      // Skip cancelled or already-deleted timers
      if (top->cancelled || _ctxState->timer_map.find(top->id) == _ctxState->timer_map.end()) {
        _ctxState->timer_heap.pop();
        // If cancelled, the timer_map entry was already removed — delete the Timer
        delete top;
        continue;
      }

      if (top->fire_at_ms > t_now) break; // not yet due

      _ctxState->timer_heap.pop();
      fired_any = true;

      if (top->cancelled) {
        delete top;
        continue;
      }

      try {
        fireTimer(top);
      } catch (...) {
        if (top->callback) {
          JSValue* cb = static_cast<JSValue*>(top->callback);
          JS_FreeValue(ctx, *cb);
          delete cb;
          top->callback = nullptr;
        }
        _ctxState->timer_map.erase(top->id);
        delete top;
        throw;
      }

      if (top->repeat && !top->cancelled) {
        // Re-arm: keep same interval from original schedule
        int64_t next = top->fire_at_ms + top->interval_ms;
        if (next <= now_ms()) next = now_ms() + top->interval_ms;
        top->fire_at_ms = next;
        _ctxState->timer_heap.push(top);
      } else {
        // Remove from map and free
        _ctxState->timer_map.erase(top->id);
        if (top->callback) {
          JSValue* cb = static_cast<JSValue*>(top->callback);
          JS_FreeValue(ctx, *cb);
          delete cb;
          top->callback = nullptr;
        }
        delete top;
      }
    }

    // 4. Check if there are more timers waiting
    bool has_active_timers = false;
    {
      // Peek at non-cancelled timers
      auto& heap = _ctxState->timer_heap;
      // We can't peek without modifying. Check timer_map instead.
      for (auto& [id, t] : _ctxState->timer_map) {
        if (!t->cancelled) { has_active_timers = true; break; }
      }
    }

    if (!has_active_timers && !fired_any) {
      // No timers, no new microtasks — done
      break;
    }

    if (has_active_timers && !fired_any) {
      // Sleep until next timer fires
      Timer* next_timer = nullptr;
      int64_t min_fire = INT64_MAX;
      for (auto& [id, t] : _ctxState->timer_map) {
        if (!t->cancelled && t->fire_at_ms < min_fire) {
          min_fire = t->fire_at_ms;
          next_timer = t;
        }
      }
      if (next_timer) {
        int64_t sleep_ms = min_fire - now_ms();
        if (sleep_ms > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
      }
    }
    // else fired_any — loop back to drain microtasks spawned by those callbacks
  }
}

// ── fireTimer ─────────────────────────────────────────────────────────────────

void QuickJSRuntime::fireTimer(Timer* t) {
  JSContext* ctx = reinterpret_cast<JSContext*>(_context);
  if (!t->callback) return;
  JSValue* cb = static_cast<JSValue*>(t->callback);
  JSValue ret = JS_Call(ctx, *cb, JS_UNDEFINED, 0, nullptr);
  if (JS_IsException(ret)) {
    JS_FreeValue(ctx, ret);
    // Mark timer as cancelled so it doesn't re-arm
    t->cancelled = true;
    JSValue ex = JS_GetException(ctx);
    std::string err = "Timer callback error";
    JSValue msg_prop = JS_GetPropertyStr(ctx, ex, "message");
    if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
      const char* s = JS_ToCString(ctx, msg_prop);
      if (s) { err = s; JS_FreeCString(ctx, s); }
    }
    JS_FreeValue(ctx, msg_prop);
    if (err == "Timer callback error") {
      JSValue str_val = JS_ToString(ctx, ex);
      if (!JS_IsException(str_val)) {
        const char* s = JS_ToCString(ctx, str_val);
        if (s) { err = s; JS_FreeCString(ctx, s); }
      }
      JS_FreeValue(ctx, str_val);
    }
    JS_FreeValue(ctx, ex);
    throw std::runtime_error(err);
  }
  JS_FreeValue(ctx, ret);
}

// ── evaluate ──────────────────────────────────────────────────────────────────

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

    JSValue msg_prop = JS_GetPropertyStr(ctx, ex, "message");
    if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
      const char* s = JS_ToCString(ctx, msg_prop);
      if (s) { err = s; JS_FreeCString(ctx, s); }
    }
    JS_FreeValue(ctx, msg_prop);

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

  // Drain the event loop (microtasks + timers) before resolving the result
  try {
    drainEventLoop();
  } catch (...) {
    JS_FreeValue(ctx, result);
    throw;
  }

  // If the script returned a Promise, extract its settled value.
  // drainEventLoop() already handled unhandled rejections via pending_rejection,
  // so here we only need to unwrap FULFILLED promises.
  if (JS_IsObject(result)) {
    JSPromiseStateEnum state = JS_PromiseState(ctx, result);
    if (state == JS_PROMISE_FULFILLED) {
      JSValue resolved = JS_PromiseResult(ctx, result);
      JS_FreeValue(ctx, result);
      result = resolved;
    } else if (state == JS_PROMISE_REJECTED) {
      // Fallback: rejection that wasn't caught by pending_rejection tracker
      JSValue reason = JS_PromiseResult(ctx, result);
      JS_FreeValue(ctx, result);
      std::string err = "Promise rejected";
      JSValue msg = JS_GetPropertyStr(ctx, reason, "message");
      if (!JS_IsException(msg) && !JS_IsUndefined(msg)) {
        const char* s = JS_ToCString(ctx, msg);
        if (s) { err = s; JS_FreeCString(ctx, s); }
      }
      JS_FreeValue(ctx, msg);
      JS_FreeValue(ctx, reason);
      throw std::runtime_error(err);
    }
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
