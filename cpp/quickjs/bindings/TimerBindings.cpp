#include "TimerBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include <chrono>

namespace margelo::nitro::nitrojsdom {

namespace {

int64_t dom_now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

JSValue js_setTimeout(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_NewInt32(ctx, 0);

  int32_t delay_ms = 0;
  if (argc >= 2) JS_ToInt32(ctx, &delay_ms, argv[1]);
  if (delay_ms < 0) delay_ms = 0;

  uint32_t id = rctx->next_timer_id++;
  Timer* t = new Timer();
  t->id = id;
  t->repeat = false;
  t->interval_ms = delay_ms;
  t->fire_at_ms = dom_now_ms() + delay_ms;
  t->callback = new JSValue(JS_DupValue(ctx, argv[0]));
  t->cancelled = false;

  rctx->timer_map[id] = t;
  rctx->timer_heap.push(t);
  return JS_NewInt32(ctx, (int32_t)id);
}

JSValue js_setInterval(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_NewInt32(ctx, 0);

  int32_t interval_ms = 0;
  if (argc >= 2) JS_ToInt32(ctx, &interval_ms, argv[1]);
  if (interval_ms < 1) interval_ms = 1;

  uint32_t id = rctx->next_timer_id++;
  Timer* t = new Timer();
  t->id = id;
  t->repeat = true;
  t->interval_ms = interval_ms;
  t->fire_at_ms = dom_now_ms() + interval_ms;
  t->callback = new JSValue(JS_DupValue(ctx, argv[0]));
  t->cancelled = false;

  rctx->timer_map[id] = t;
  rctx->timer_heap.push(t);
  return JS_NewInt32(ctx, (int32_t)id);
}

JSValue js_clearTimer(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_UNDEFINED;
  auto* rctx = get_ctx(ctx);
  if (!rctx) return JS_UNDEFINED;

  uint32_t id = 0;
  JS_ToUint32(ctx, &id, argv[0]);
  auto it = rctx->timer_map.find(id);
  if (it != rctx->timer_map.end()) {
    it->second->cancelled = true;
    // Free the callback now to save memory; mark as null
    if (it->second->callback) {
      JSValue* cb = static_cast<JSValue*>(it->second->callback);
      JS_FreeValue(ctx, *cb);
      delete cb;
      it->second->callback = nullptr;
    }
    rctx->timer_map.erase(it);
  }
  return JS_UNDEFINED;
}

} // namespace

void TimerBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "setTimeout",    JS_NewCFunction(ctx, js_setTimeout,  "setTimeout",   2));
  JS_SetPropertyStr(ctx, global, "setInterval",   JS_NewCFunction(ctx, js_setInterval, "setInterval",  2));
  JS_SetPropertyStr(ctx, global, "clearTimeout",  JS_NewCFunction(ctx, js_clearTimer,  "clearTimeout", 1));
  JS_SetPropertyStr(ctx, global, "clearInterval", JS_NewCFunction(ctx, js_clearTimer,  "clearInterval",1));
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
