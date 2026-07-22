#include "WindowBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

// ── console ────────────────────────────────────────────────────────────────

JSValue js_console_method(JSContext* ctx, JSValue, int argc, JSValue* argv, int magic) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->console_callback) return JS_UNDEFINED;

  static const char* levels[] = { "log", "warn", "error", "info", "debug" };
  std::string level = (magic >= 0 && magic < 5) ? levels[magic] : "log";

  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; i++) {
    JSValue str_val = JS_ToString(ctx, argv[i]);
    const char* s = JS_ToCString(ctx, str_val);
    args.push_back(s ? s : "");
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, str_val);
  }

  rctx->console_callback(level, args);
  return JS_UNDEFINED;
}

// ── window.alert / confirm / prompt ───────────────────────────────────────────

JSValue js_window_alert(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->alert_callback) return JS_UNDEFINED;
  std::string message;
  if (argc >= 1) {
    JSValue str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { message = s; JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  try { rctx->alert_callback(message); }
  catch (const std::exception& e) { return JS_ThrowInternalError(ctx, "alert callback threw: %s", e.what()); }
  catch (...) { return JS_ThrowInternalError(ctx, "alert callback threw an unknown exception"); }
  return JS_UNDEFINED;
}

JSValue js_window_confirm(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->confirm_callback) return JS_FALSE;
  std::string message;
  if (argc >= 1) {
    JSValue str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { message = s; JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  bool result;
  try { result = rctx->confirm_callback(message); }
  catch (const std::exception& e) { return JS_ThrowInternalError(ctx, "confirm callback threw: %s", e.what()); }
  catch (...) { return JS_ThrowInternalError(ctx, "confirm callback threw an unknown exception"); }
  return JS_NewBool(ctx, result);
}

JSValue js_window_prompt(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->prompt_callback) return JS_NULL;
  std::string message;
  if (argc >= 1) {
    JSValue str_val = JS_ToString(ctx, argv[0]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { message = s; JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  std::optional<std::string> defaultValue;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    JSValue str_val = JS_ToString(ctx, argv[1]);
    if (JS_IsException(str_val)) return JS_EXCEPTION;
    const char* s = JS_ToCString(ctx, str_val);
    if (s) { defaultValue = std::string(s); JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, str_val);
  }
  std::optional<std::string> result;
  try { result = rctx->prompt_callback(message, defaultValue); }
  catch (const std::exception& e) { return JS_ThrowInternalError(ctx, "prompt callback threw: %s", e.what()); }
  catch (...) { return JS_ThrowInternalError(ctx, "prompt callback threw an unknown exception"); }
  if (result.has_value()) {
    return JS_NewStringLen(ctx, result->c_str(), result->size());
  }
  return JS_NULL;
}

// ── window.location ───────────────────────────────────────────────────────────

const char* kLocationBootstrapScript = R"JS(
(function() {
  function parseUrl(str) {
    str = String(str === undefined || str === null ? '' : str);
    var m = /^([^:\/?#]+:)(?:\/\/(?:[^\/?#@]*@)?([^\/?#:]*)(?::(\d+))?)?([^?#]*)(\?[^#]*)?(#.*)?$/.exec(str);
    if (!m) {
      return { protocol: '', hostname: '', port: '', pathname: '', search: '', hash: '' };
    }
    var protocol = m[1] || '';
    var hostname = m[2] || '';
    var port = m[3] || '';
    var pathname = m[4] || '';
    var search = m[5] || '';
    var hash = m[6] || '';
    if (!pathname && hostname) pathname = '/';
    return { protocol: protocol, hostname: hostname, port: port, pathname: pathname, search: search, hash: hash };
  }

  function Location(initialHref) {
    this._href = String(initialHref || 'about:blank');
  }

  function defineParsedProp(name) {
    Object.defineProperty(Location.prototype, name, {
      get: function() { return parseUrl(this._href)[name]; },
      enumerable: true,
      configurable: true,
    });
  }
  ['protocol', 'hostname', 'port', 'pathname', 'search', 'hash'].forEach(defineParsedProp);

  Object.defineProperty(Location.prototype, 'host', {
    get: function() {
      var p = parseUrl(this._href);
      return p.hostname + (p.port ? ':' + p.port : '');
    },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(Location.prototype, 'origin', {
    get: function() {
      var p = parseUrl(this._href);
      if (!p.hostname) return 'null';
      return p.protocol + '//' + p.hostname + (p.port ? ':' + p.port : '');
    },
    enumerable: true,
    configurable: true,
  });

  function resolveUrl(base, next) {
    next = String(next);
    if (/^[a-zA-Z][a-zA-Z0-9+\-.]*:/.test(next)) return next;
    var p = parseUrl(base);
    var origin = (p.protocol || '') + (p.hostname ? '//' + p.hostname + (p.port ? ':' + p.port : '') : '');
    if (next.charAt(0) === '/') return origin + next;
    if (next.charAt(0) === '#') return base.split('#')[0] + next;
    if (next.charAt(0) === '?') return base.split('?')[0].split('#')[0] + next;
    var dir = (p.pathname || '/').replace(/\/[^\/]*$/, '/');
    return origin + dir + next;
  }

  Object.defineProperty(Location.prototype, 'href', {
    get: function() { return this._href; },
    set: function(v) { this._href = resolveUrl(this._href, v); },
    enumerable: true,
    configurable: true,
  });

  Location.prototype.toString = function() { return this._href; };
  Location.prototype.assign = function(url) { this._href = resolveUrl(this._href, url); };
  Location.prototype.replace = function(url) { this._href = resolveUrl(this._href, url); };
  Location.prototype.reload = function() {};

  globalThis.Location = Location;
  globalThis.location = new Location(globalThis.__initialHref);
  delete globalThis.__initialHref;
})();
)JS";

// ── crypto.getRandomValues ────────────────────────────────────────────────────

const char* kCryptoBootstrapScript = R"JS(
(function() {
  globalThis.crypto = globalThis.crypto || {};
  globalThis.crypto.getRandomValues = function(typedArray) {
    if (!typedArray || typeof typedArray.length !== 'number' || typedArray instanceof BigInt64Array || typedArray instanceof BigUint64Array) {
      throw new TypeError('crypto.getRandomValues() requires an integer TypedArray argument');
    }
    if (typedArray.length > 65536) {
      var err = new Error('crypto.getRandomValues() can only fill up to 65536 bytes at a time');
      err.name = 'QuotaExceededError';
      throw err;
    }
    for (var i = 0; i < typedArray.length; i++) {
      typedArray[i] = Math.floor(Math.random() * 0x100000000);
    }
    return typedArray;
  };
})();
)JS";

} // namespace

void WindowBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);

  JS_SetPropertyStr(ctx, global, "alert",   JS_NewCFunction(ctx, js_window_alert,   "alert",   1));
  JS_SetPropertyStr(ctx, global, "confirm", JS_NewCFunction(ctx, js_window_confirm, "confirm", 1));
  JS_SetPropertyStr(ctx, global, "prompt",  JS_NewCFunction(ctx, js_window_prompt,  "prompt",  2));

  JSValue console_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console_obj, "log",   JS_NewCFunctionMagic(ctx, js_console_method, "log",   0, JS_CFUNC_generic_magic, 0));
  JS_SetPropertyStr(ctx, console_obj, "warn",  JS_NewCFunctionMagic(ctx, js_console_method, "warn",  0, JS_CFUNC_generic_magic, 1));
  JS_SetPropertyStr(ctx, console_obj, "error", JS_NewCFunctionMagic(ctx, js_console_method, "error", 0, JS_CFUNC_generic_magic, 2));
  JS_SetPropertyStr(ctx, console_obj, "info",  JS_NewCFunctionMagic(ctx, js_console_method, "info",  0, JS_CFUNC_generic_magic, 3));
  JS_SetPropertyStr(ctx, console_obj, "debug", JS_NewCFunctionMagic(ctx, js_console_method, "debug", 0, JS_CFUNC_generic_magic, 4));
  JS_SetPropertyStr(ctx, global, "console", console_obj);

  JS_FreeValue(ctx, global);

  JSValue location_result = JS_Eval(ctx, kLocationBootstrapScript, strlen(kLocationBootstrapScript),
                                     "<location-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(location_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, location_result);

  JSValue crypto_result = JS_Eval(ctx, kCryptoBootstrapScript, strlen(kCryptoBootstrapScript),
                                   "<crypto-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(crypto_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, crypto_result);
}

} // namespace margelo::nitro::nitrojsdom
