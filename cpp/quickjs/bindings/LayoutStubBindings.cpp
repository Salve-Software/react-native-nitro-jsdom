#include "LayoutStubBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kLayoutStubBootstrapScript = R"JS(
(function() {
  function definePositiveInt(proto, name) {
    Object.defineProperty(proto, name, { get: function() { return 0; }, configurable: true });
  }

  // ── offset*/client*/scroll* dimensions ────────────────────────────────
  ['offsetWidth', 'offsetHeight', 'offsetTop', 'offsetLeft',
   'clientWidth', 'clientHeight', 'clientTop', 'clientLeft',
   'scrollWidth', 'scrollHeight'].forEach(function(name) {
    definePositiveInt(Element.prototype, name);
  });
  Object.defineProperty(Element.prototype, 'offsetParent', {
    get: function() { return null; },
    configurable: true,
  });

  // scrollTop/scrollLeft are real per-element state (jsdom does the same) —
  // nothing reads them back to affect layout, but round-tripping a value a
  // script just set is more useful than hardcoding 0.
  Object.defineProperty(Element.prototype, 'scrollTop', {
    get: function() { return this._scrollTop || 0; },
    set: function(v) { this._scrollTop = Number(v) || 0; },
    configurable: true,
  });
  Object.defineProperty(Element.prototype, 'scrollLeft', {
    get: function() { return this._scrollLeft || 0; },
    set: function(v) { this._scrollLeft = Number(v) || 0; },
    configurable: true,
  });

  // ── scrollIntoView / scrollTo / scrollBy / scroll ─────────────────────
  Element.prototype.scrollIntoView = function() {};
  Element.prototype.scrollTo = function() {};
  Element.prototype.scrollBy = function() {};
  Element.prototype.scroll = function() {};

  globalThis.scrollX = 0;
  globalThis.scrollY = 0;
  globalThis.pageXOffset = 0;
  globalThis.pageYOffset = 0;
  globalThis.scrollTo = function() {};
  globalThis.scrollBy = function() {};
  globalThis.scroll = function() {};

  // ── document.elementFromPoint / elementsFromPoint ─────────────────────
  document.elementFromPoint = function() { return null; };
  document.elementsFromPoint = function() { return []; };

  // ── ResizeObserver / IntersectionObserver ─────────────────────────────
  function ResizeObserver(callback) {
    if (typeof callback !== 'function') {
      throw new TypeError("Failed to construct 'ResizeObserver': parameter 1 is not a function.");
    }
    this._callback = callback;
  }
  ResizeObserver.prototype.observe = function() {};
  ResizeObserver.prototype.unobserve = function() {};
  ResizeObserver.prototype.disconnect = function() {};
  globalThis.ResizeObserver = ResizeObserver;

  function normalizeThreshold(threshold) {
    if (threshold === undefined) return [0];
    var arr = Array.isArray(threshold) ? threshold.slice() : [threshold];
    return arr.length ? arr : [0];
  }

  function IntersectionObserver(callback, options) {
    if (typeof callback !== 'function') {
      throw new TypeError("Failed to construct 'IntersectionObserver': parameter 1 is not a function.");
    }
    options = options || {};
    this._callback = callback;
    this.root = options.root || null;
    this.rootMargin = options.rootMargin || '0px';
    this.thresholds = normalizeThreshold(options.threshold);
  }
  IntersectionObserver.prototype.observe = function() {};
  IntersectionObserver.prototype.unobserve = function() {};
  IntersectionObserver.prototype.disconnect = function() {};
  IntersectionObserver.prototype.takeRecords = function() { return []; };
  globalThis.IntersectionObserver = IntersectionObserver;
})();
)JS";

} // namespace

void LayoutStubBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kLayoutStubBootstrapScript, strlen(kLayoutStubBootstrapScript),
                            "<layout-stub-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
