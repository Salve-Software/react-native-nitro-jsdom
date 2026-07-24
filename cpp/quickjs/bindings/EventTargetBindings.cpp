#include "EventTargetBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kEventTargetBootstrapScript = R"JS(
(function() {
  function EventTarget() {
    this._listeners = Object.create(null);
  }

  EventTarget.prototype.addEventListener = function(type, listener, options) {
    if (typeof listener !== 'function' && !(listener && typeof listener.handleEvent === 'function')) return;
    type = String(type);
    var capture = !!(options === true || (options && options.capture));
    var once = !!(options && options.once);
    var list = this._listeners[type] || (this._listeners[type] = []);
    for (var i = 0; i < list.length; i++) {
      if (list[i].listener === listener && list[i].capture === capture) return;
    }
    list.push({ listener: listener, capture: capture, once: once });
  };

  EventTarget.prototype.removeEventListener = function(type, listener, options) {
    var list = this._listeners[String(type)];
    if (!list) return;
    var capture = !!(options === true || (options && options.capture));
    for (var i = 0; i < list.length; i++) {
      if (list[i].listener === listener && list[i].capture === capture) { list.splice(i, 1); return; }
    }
  };

  EventTarget.prototype.dispatchEvent = function(event) {
    var list = this._listeners[event.type];
    Object.defineProperty(event, 'target', { value: this, configurable: true });
    Object.defineProperty(event, 'currentTarget', { value: this, configurable: true });
    try {
      if (list) {
        var snapshot = list.slice();
        for (var i = 0; i < snapshot.length; i++) {
          var entry = snapshot[i];
          if (list.indexOf(entry) === -1) continue;
          if (entry.once) {
            var idx = list.indexOf(entry);
            if (idx !== -1) list.splice(idx, 1);
          }
          if (typeof entry.listener === 'function') entry.listener.call(this, event);
          else entry.listener.handleEvent(event);
          if (event.__immediatePropagationStopped) break;
        }
      }
    } finally {
      Object.defineProperty(event, 'currentTarget', { value: null, configurable: true });
    }
    return !event.defaultPrevented;
  };

  globalThis.EventTarget = EventTarget;
})();
)JS";

} // namespace

void EventTargetBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kEventTargetBootstrapScript, strlen(kEventTargetBootstrapScript),
                            "<event-target-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
