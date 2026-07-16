#pragma once

// fetch()/Headers/Response/XMLHttpRequest. The native side (__nativeFetchSync)
// bridges to the RN onFetch callback synchronously; the JS-level bootstrap
// script builds the spec-shaped wrapper objects on top of it.

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class FetchBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
