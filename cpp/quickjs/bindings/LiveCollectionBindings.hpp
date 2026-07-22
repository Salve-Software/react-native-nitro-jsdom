#pragma once

#include "quickjs.h"
#include <string>

namespace margelo::nitro::nitrojsdom {

class LiveCollectionBindings {
public:
  static void install(JSContext* ctx);
  static JSValue makeChildren(JSContext* ctx, void* element);
  static JSValue makeChildNodes(JSContext* ctx, void* node);
  static JSValue makeBySelector(JSContext* ctx, void* owner_or_null, const std::string& selector);
};

} // namespace margelo::nitro::nitrojsdom
