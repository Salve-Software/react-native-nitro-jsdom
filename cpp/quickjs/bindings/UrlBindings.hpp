#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

class UrlBindings {
public:
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
