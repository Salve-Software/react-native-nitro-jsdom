#pragma once

// globalThis.EventTarget — a standalone, constructible EventTarget for
// scripts that want addEventListener/removeEventListener/dispatchEvent
// pub-sub semantics without extending an Element/document/window (which
// already have their own dispatch paths — see EventBindings). Pure JS: its
// own per-instance listener map, no bubbling (a bare EventTarget has no
// ancestors to bubble through), reusing the Event/CustomEvent classes
// EventBindings already installs.
//
// Must run after EventBindings (globalThis.Event, DOMException already
// available for the "listener not a function" TypeError path — actually
// mirrors native EventTarget, which silently ignores a non-callable
// listener rather than throwing).

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

struct EventTargetBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
