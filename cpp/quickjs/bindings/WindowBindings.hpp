#pragma once

// window.alert/confirm/prompt (bridged to onAlert/onConfirm/onPrompt native
// callbacks), console.log/warn/error/info/debug, and the window.location,
// window.history, window.getSelection, crypto, navigator, matchMedia, and
// structuredClone bootstrap scripts.

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class WindowBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
