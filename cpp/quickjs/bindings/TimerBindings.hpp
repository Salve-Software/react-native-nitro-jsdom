#pragma once

// setTimeout/setInterval/clearTimeout/clearInterval. The timers themselves are
// fired by QuickJSRuntime's event loop (RuntimeContext::timer_heap); this file
// only exposes the JS-facing scheduling API.

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class TimerBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
