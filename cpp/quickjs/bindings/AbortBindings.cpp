#include "AbortBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kAbortBootstrapScript = R"JS(
(function() {
  function makeAbortError() {
    var e = new Error('signal is aborted without reason');
    e.name = 'AbortError';
    return e;
  }
  function makeTimeoutError() {
    var e = new Error('signal timed out');
    e.name = 'TimeoutError';
    return e;
  }

  function AbortSignal() {
    this.aborted = false;
    this.reason = undefined;
    this._listeners = [];
    this._onabort = null;
    this._onabortListener = null;
  }

  Object.defineProperty(AbortSignal.prototype, 'onabort', {
    get: function() { return this._onabort; },
    set: function(fn) {
      if (this._onabortListener) {
        var idx = this._listeners.indexOf(this._onabortListener);
        if (idx !== -1) this._listeners.splice(idx, 1);
      }
      this._onabort = typeof fn === 'function' ? fn : null;
      if (this._onabort) {
        var self = this;
        this._onabortListener = function(evt) { self._onabort(evt); };
        this._listeners.push(this._onabortListener);
      } else {
        this._onabortListener = null;
      }
    },
    enumerable: true,
    configurable: true,
  });

  AbortSignal.prototype.addEventListener = function(type, cb) {
    if (type !== 'abort' || typeof cb !== 'function') return;
    if (this._listeners.indexOf(cb) === -1) this._listeners.push(cb);
  };
  AbortSignal.prototype.removeEventListener = function(type, cb) {
    if (type !== 'abort') return;
    var idx = this._listeners.indexOf(cb);
    if (idx !== -1) this._listeners.splice(idx, 1);
  };
  AbortSignal.prototype.dispatchEvent = function(evt) {
    if (evt && evt.type === 'abort') this._fire();
    return true;
  };
  AbortSignal.prototype.throwIfAborted = function() {
    if (this.aborted) throw this.reason;
  };
  AbortSignal.prototype._fire = function() {
    var evt = new Event('abort');
    evt.target = this;
    this._listeners.slice().forEach(function(cb) {
      try { cb(evt); } catch (e) {}
    });
  };
  AbortSignal.prototype._abort = function(reason) {
    if (this.aborted) return;
    this.aborted = true;
    this.reason = reason !== undefined ? reason : makeAbortError();
    this._fire();
  };

  AbortSignal.abort = function(reason) {
    var s = new AbortSignal();
    s._abort(reason);
    return s;
  };
  AbortSignal.timeout = function(ms) {
    var s = new AbortSignal();
    setTimeout(function() { s._abort(makeTimeoutError()); }, ms);
    return s;
  };
  AbortSignal.any = function(signals) {
    var s = new AbortSignal();
    var list = [];
    for (var i = 0; i < signals.length; i++) list.push(signals[i]);
    for (var j = 0; j < list.length; j++) {
      if (list[j].aborted) { s._abort(list[j].reason); break; }
    }
    if (!s.aborted) {
      list.forEach(function(signal) {
        signal.addEventListener('abort', function() { s._abort(signal.reason); });
      });
    }
    return s;
  };

  globalThis.AbortSignal = AbortSignal;

  function AbortController() {
    this.signal = new AbortSignal();
  }
  AbortController.prototype.abort = function(reason) {
    this.signal._abort(reason);
  };

  globalThis.AbortController = AbortController;
})();
)JS";

} // namespace

void AbortBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kAbortBootstrapScript, strlen(kAbortBootstrapScript),
                            "<abort-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
