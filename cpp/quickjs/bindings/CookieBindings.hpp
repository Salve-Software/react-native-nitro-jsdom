#pragma once

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// Registers document.cookie (get/set), backed by an in-memory per-runtime
// cookie jar (RuntimeContext::cookie_jar — a name -> value map preserving
// insertion order). jsdom backs this with a real tough-cookie CookieJar
// scoped by domain/path/expiry; this sandbox has no navigation or multiple
// origins, so scoping attributes (path/domain/secure/samesite) are parsed
// off and discarded rather than enforced. `expires`/`max-age` are the
// exception: a value in the past (or max-age <= 0) is treated as the
// deletion idiom real-world scripts use it for, and removes the cookie
// instead of leaving a stray "name=" entry in the jar. `expires` parsing is
// best-effort (RFC 1123/850/asctime formats only, not a full HTTP-date parser).
//
// Must run after DocumentBindings (needs globalThis.document to exist).
struct CookieBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
