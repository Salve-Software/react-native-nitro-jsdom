#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers document.cookie (get/set), backed by an in-memory per-runtime
// cookie jar (RuntimeContext::cookie_jar — a name -> value map preserving
// insertion order). jsdom backs this with a real tough-cookie CookieJar
// scoped by domain/path/expiry; this sandbox has no navigation or multiple
// origins, so the setter parses off only the leading "name=value" pair and
// discards any attributes (expires/path/domain/secure/samesite) rather than
// enforcing them.
//
// Must run after DocumentBindings (needs globalThis.document to exist).
struct CookieBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
