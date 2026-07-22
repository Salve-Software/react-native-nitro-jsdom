#include "WindowBindings.hpp"
#include "DOMExceptionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

// ── atob / btoa ────────────────────────────────────────────────────────────

const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int8_t base64_decode_char(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool is_ascii_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
}

JSValue js_window_btoa(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_ThrowTypeError(ctx, "btoa() requires 1 argument");
  JSValue str_val = JS_ToString(ctx, argv[0]);
  if (JS_IsException(str_val)) return JS_EXCEPTION;
  const char* s = JS_ToCString(ctx, str_val);
  if (!s) { JS_FreeValue(ctx, str_val); return JS_EXCEPTION; }

  // JS_ToCString yields UTF-8; btoa operates on Latin1 code units, so decode
  // the UTF-8 back into code points and reject anything outside 0x00-0xFF.
  std::vector<uint8_t> bytes;
  size_t len = strlen(s);
  bytes.reserve(len);
  bool out_of_range = false;
  for (size_t i = 0; i < len && !out_of_range;) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    uint32_t codepoint;
    size_t seq_len;
    if (c < 0x80) { codepoint = c; seq_len = 1; }
    else if ((c & 0xE0) == 0xC0 && i + 1 < len) { codepoint = c & 0x1F; seq_len = 2; }
    else if ((c & 0xF0) == 0xE0 && i + 2 < len) { codepoint = c & 0x0F; seq_len = 3; }
    else { out_of_range = true; break; }
    for (size_t k = 1; k < seq_len; k++) {
      codepoint = (codepoint << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    }
    if (codepoint > 0xFF) { out_of_range = true; break; }
    bytes.push_back(static_cast<uint8_t>(codepoint));
    i += seq_len;
  }
  JS_FreeCString(ctx, s);
  JS_FreeValue(ctx, str_val);

  if (out_of_range) {
    return throw_dom_exception(ctx, "InvalidCharacterError",
        "The string to be encoded contains characters outside of the Latin1 range.");
  }

  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);
  size_t i = 0;
  while (i + 3 <= bytes.size()) {
    uint32_t n = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
    out += kBase64Alphabet[(n >> 18) & 0x3F];
    out += kBase64Alphabet[(n >> 12) & 0x3F];
    out += kBase64Alphabet[(n >> 6) & 0x3F];
    out += kBase64Alphabet[n & 0x3F];
    i += 3;
  }
  size_t remaining = bytes.size() - i;
  if (remaining == 1) {
    uint32_t n = bytes[i] << 16;
    out += kBase64Alphabet[(n >> 18) & 0x3F];
    out += kBase64Alphabet[(n >> 12) & 0x3F];
    out += "==";
  } else if (remaining == 2) {
    uint32_t n = (bytes[i] << 16) | (bytes[i + 1] << 8);
    out += kBase64Alphabet[(n >> 18) & 0x3F];
    out += kBase64Alphabet[(n >> 12) & 0x3F];
    out += kBase64Alphabet[(n >> 6) & 0x3F];
    out += "=";
  }

  return JS_NewStringLen(ctx, out.c_str(), out.size());
}

JSValue js_window_atob(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_ThrowTypeError(ctx, "atob() requires 1 argument");
  JSValue str_val = JS_ToString(ctx, argv[0]);
  if (JS_IsException(str_val)) return JS_EXCEPTION;
  const char* s = JS_ToCString(ctx, str_val);
  if (!s) { JS_FreeValue(ctx, str_val); return JS_EXCEPTION; }

  std::string data;
  for (const char* p = s; *p; p++) {
    if (!is_ascii_whitespace(*p)) data += *p;
  }
  JS_FreeCString(ctx, s);
  JS_FreeValue(ctx, str_val);

  if (data.size() % 4 == 0) {
    if (data.size() >= 2 && data[data.size() - 1] == '=' && data[data.size() - 2] == '=') {
      data.resize(data.size() - 2);
    } else if (!data.empty() && data.back() == '=') {
      data.resize(data.size() - 1);
    }
  }

  if (data.size() % 4 == 1) {
    return throw_dom_exception(ctx, "InvalidCharacterError",
        "The string to be decoded is not correctly encoded.");
  }
  for (char c : data) {
    if (base64_decode_char(static_cast<unsigned char>(c)) < 0) {
      return throw_dom_exception(ctx, "InvalidCharacterError",
          "The string to be decoded is not correctly encoded.");
    }
  }

  std::string out;
  out.reserve((data.size() / 4) * 3 + 3);
  size_t i = 0;
  while (i + 4 <= data.size()) {
    uint32_t n = (base64_decode_char(data[i]) << 18) | (base64_decode_char(data[i + 1]) << 12) |
                 (base64_decode_char(data[i + 2]) << 6) | base64_decode_char(data[i + 3]);
    out += static_cast<char>((n >> 16) & 0xFF);
    out += static_cast<char>((n >> 8) & 0xFF);
    out += static_cast<char>(n & 0xFF);
    i += 4;
  }
  size_t remaining = data.size() - i;
  if (remaining == 2) {
    uint32_t n = (base64_decode_char(data[i]) << 18) | (base64_decode_char(data[i + 1]) << 12);
    out += static_cast<char>((n >> 16) & 0xFF);
  } else if (remaining == 3) {
    uint32_t n = (base64_decode_char(data[i]) << 18) | (base64_decode_char(data[i + 1]) << 12) |
                 (base64_decode_char(data[i + 2]) << 6);
    out += static_cast<char>((n >> 16) & 0xFF);
    out += static_cast<char>((n >> 8) & 0xFF);
  }

  // atob's result is a "binary string" — each output char code is one decoded
  // byte (0x00-0xFF). JS_NewStringLen expects UTF-8 input, so each byte must
  // be re-encoded as the UTF-8 sequence for that same code point (1 byte for
  // 0x00-0x7F, 2 bytes for 0x80-0xFF) to round-trip through QuickJS correctly.
  std::string utf8;
  utf8.reserve(out.size() * 2);
  for (unsigned char byte : out) {
    if (byte < 0x80) {
      utf8 += static_cast<char>(byte);
    } else {
      utf8 += static_cast<char>(0xC0 | (byte >> 6));
      utf8 += static_cast<char>(0x80 | (byte & 0x3F));
    }
  }
  return JS_NewStringLen(ctx, utf8.c_str(), utf8.size());
}

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

JSValue js_native_random_bytes(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  int32_t count = 0;
  if (argc >= 1) JS_ToInt32(ctx, &count, argv[0]);
  if (count < 0) count = 0;

  static thread_local std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<int> dist(0, 255);

  JSValue arr = JS_NewArray(ctx);
  for (int32_t i = 0; i < count; i++) {
    JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), JS_NewInt32(ctx, dist(gen)));
  }
  return arr;
}

const char* kCryptoBootstrapScript = R"JS(
(function() {
  globalThis.crypto = globalThis.crypto || {};
  globalThis.crypto.getRandomValues = function(typedArray) {
    if (!typedArray || typeof typedArray.byteLength !== 'number' || typedArray instanceof BigInt64Array || typedArray instanceof BigUint64Array) {
      throw new TypeError('crypto.getRandomValues() requires an integer TypedArray argument');
    }
    if (typedArray.byteLength > 65536) {
      var err = new Error('crypto.getRandomValues() can only fill up to 65536 bytes at a time');
      err.name = 'QuotaExceededError';
      throw err;
    }
    var bytes = __nativeRandomBytes(typedArray.byteLength);
    var view = new Uint8Array(typedArray.buffer, typedArray.byteOffset, typedArray.byteLength);
    for (var i = 0; i < bytes.length; i++) view[i] = bytes[i];
    return typedArray;
  };
})();
)JS";

// ── navigator ──────────────────────────────────────────────────────────────

const char* kNavigatorBootstrapScript = R"JS(
(function() {
  var navigator = {
    userAgent: 'Mozilla/5.0 (ReactNative) react-native-nitro-jsdom',
    platform: 'ReactNative',
    language: 'en-US',
    languages: ['en-US'],
    onLine: true,
    cookieEnabled: false,
    hardwareConcurrency: 1,
  };
  globalThis.navigator = navigator;
})();
)JS";

// ── matchMedia ─────────────────────────────────────────────────────────────
// No layout engine backs this sandbox, so every query reports "no match" —
// mirrors jsdom's own stance that layout-dependent APIs return inert defaults
// rather than throwing.

const char* kMatchMediaBootstrapScript = R"JS(
(function() {
  function MediaQueryList(media) {
    this.media = String(media === undefined ? '' : media);
    this.matches = false;
    this.onchange = null;
    this._listeners = [];
  }
  MediaQueryList.prototype.addListener = function(cb) {
    if (typeof cb === 'function' && this._listeners.indexOf(cb) === -1) this._listeners.push(cb);
  };
  MediaQueryList.prototype.removeListener = function(cb) {
    var idx = this._listeners.indexOf(cb);
    if (idx !== -1) this._listeners.splice(idx, 1);
  };
  MediaQueryList.prototype.addEventListener = function(type, cb) {
    if (type === 'change') this.addListener(cb);
  };
  MediaQueryList.prototype.removeEventListener = function(type, cb) {
    if (type === 'change') this.removeListener(cb);
  };
  MediaQueryList.prototype.dispatchEvent = function() { return true; };

  globalThis.matchMedia = function(query) {
    return new MediaQueryList(query);
  };
})();
)JS";

} // namespace

void WindowBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);

  JS_SetPropertyStr(ctx, global, "alert",   JS_NewCFunction(ctx, js_window_alert,   "alert",   1));
  JS_SetPropertyStr(ctx, global, "confirm", JS_NewCFunction(ctx, js_window_confirm, "confirm", 1));
  JS_SetPropertyStr(ctx, global, "prompt",  JS_NewCFunction(ctx, js_window_prompt,  "prompt",  2));
  JS_SetPropertyStr(ctx, global, "atob",    JS_NewCFunction(ctx, js_window_atob,    "atob",    1));
  JS_SetPropertyStr(ctx, global, "btoa",    JS_NewCFunction(ctx, js_window_btoa,    "btoa",    1));

  JSValue console_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console_obj, "log",   JS_NewCFunctionMagic(ctx, js_console_method, "log",   0, JS_CFUNC_generic_magic, 0));
  JS_SetPropertyStr(ctx, console_obj, "warn",  JS_NewCFunctionMagic(ctx, js_console_method, "warn",  0, JS_CFUNC_generic_magic, 1));
  JS_SetPropertyStr(ctx, console_obj, "error", JS_NewCFunctionMagic(ctx, js_console_method, "error", 0, JS_CFUNC_generic_magic, 2));
  JS_SetPropertyStr(ctx, console_obj, "info",  JS_NewCFunctionMagic(ctx, js_console_method, "info",  0, JS_CFUNC_generic_magic, 3));
  JS_SetPropertyStr(ctx, console_obj, "debug", JS_NewCFunctionMagic(ctx, js_console_method, "debug", 0, JS_CFUNC_generic_magic, 4));
  JS_SetPropertyStr(ctx, global, "console", console_obj);
  JS_SetPropertyStr(ctx, global, "__nativeRandomBytes", JS_NewCFunction(ctx, js_native_random_bytes, "__nativeRandomBytes", 1));

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

  JSValue navigator_result = JS_Eval(ctx, kNavigatorBootstrapScript, strlen(kNavigatorBootstrapScript),
                                      "<navigator-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(navigator_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, navigator_result);

  JSValue match_media_result = JS_Eval(ctx, kMatchMediaBootstrapScript, strlen(kMatchMediaBootstrapScript),
                                        "<match-media-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(match_media_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, match_media_result);
}

} // namespace margelo::nitro::nitrojsdom
