#include "WindowBindings.hpp"
#include "DOMExceptionBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include <cstdlib>
#include <cstring>
#include <optional>
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
  size_t len = 0;
  const char* s = JS_ToCStringLen(ctx, &len, str_val);
  if (!s) { JS_FreeValue(ctx, str_val); return JS_EXCEPTION; }

  // JS_ToCStringLen yields UTF-8 (length-aware, so embedded NULs survive);
  // btoa operates on Latin1 code units, so decode the UTF-8 back into code
  // points and reject anything outside 0x00-0xFF.
  std::vector<uint8_t> bytes;
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
  size_t len = 0;
  const char* s = JS_ToCStringLen(ctx, &len, str_val);
  if (!s) { JS_FreeValue(ctx, str_val); return JS_EXCEPTION; }

  std::string data;
  data.reserve(len);
  for (size_t i = 0; i < len; i++) {
    if (!is_ascii_whitespace(s[i])) data += s[i];
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

  std::vector<uint8_t> bytes(static_cast<size_t>(count));
  if (count > 0) arc4random_buf(bytes.data(), bytes.size());

  JSValue arr = JS_NewArray(ctx);
  for (int32_t i = 0; i < count; i++) {
    JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), JS_NewInt32(ctx, bytes[static_cast<size_t>(i)]));
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
  globalThis.crypto.randomUUID = function() {
    var bytes = __nativeRandomBytes(16);
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    var hex = [];
    for (var i = 0; i < 16; i++) {
      var h = bytes[i].toString(16);
      hex.push(h.length === 1 ? '0' + h : h);
    }
    return hex.slice(0, 4).join('') + '-' + hex.slice(4, 6).join('') + '-' +
      hex.slice(6, 8).join('') + '-' + hex.slice(8, 10).join('') + '-' + hex.slice(10, 16).join('');
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

// ── structuredClone ────────────────────────────────────────────────────────

const char* kStructuredCloneBootstrapScript = R"JS(
(function() {
  globalThis.structuredClone = function(value) {
    var seen = new Map();
    function clone(v) {
      if (v === null || typeof v !== 'object') {
        if (typeof v === 'function' || typeof v === 'symbol') {
          throw new DOMException(String(v) + ' could not be cloned.', 'DataCloneError');
        }
        return v;
      }
      if (seen.has(v)) return seen.get(v);
      if (v instanceof Date) return new Date(v.getTime());
      if (v instanceof RegExp) return new RegExp(v.source, v.flags);
      if (Array.isArray(v)) {
        var arr = [];
        seen.set(v, arr);
        for (var i = 0; i < v.length; i++) arr.push(clone(v[i]));
        return arr;
      }
      if (v instanceof Map) {
        var m = new Map();
        seen.set(v, m);
        v.forEach(function(val, key) { m.set(clone(key), clone(val)); });
        return m;
      }
      if (v instanceof Set) {
        var s = new Set();
        seen.set(v, s);
        v.forEach(function(val) { s.add(clone(val)); });
        return s;
      }
      if (typeof ArrayBuffer !== 'undefined' && ArrayBuffer.isView && ArrayBuffer.isView(v)) {
        return new v.constructor(v);
      }
      if (typeof ArrayBuffer !== 'undefined' && v instanceof ArrayBuffer) {
        return v.slice(0);
      }
      var proto = Object.getPrototypeOf(v);
      if (proto !== Object.prototype && proto !== null) {
        throw new DOMException('The object could not be cloned.', 'DataCloneError');
      }
      var out = {};
      seen.set(v, out);
      for (var key in v) {
        if (Object.prototype.hasOwnProperty.call(v, key)) out[key] = clone(v[key]);
      }
      return out;
    }
    return clone(value);
  };
})();
)JS";

// ── window.history ────────────────────────────────────────────────────────
// No real page navigation exists in this sandbox, so History tracks its own
// in-memory entry stack rather than session history. pushState/replaceState
// never fire popstate (per spec); go/back/forward do, since those are the
// events CMS-widget routers actually listen for. `title` is accepted but
// ignored (same as jsdom — nothing renders a document title). State is
// structuredClone()'d on write (per spec — matches pushState/replaceState's
// documented "serializable" semantics), so callers mutating their object
// after the call can't leak into a stored entry. `go()`'s delta is truncated
// to an integer so a fractional/NaN input can't produce a non-integer
// `_index`.

const char* kHistoryBootstrapScript = R"JS(
(function() {
  function History() {
    this._entries = [null];
    this._index = 0;
  }
  Object.defineProperty(History.prototype, 'length', {
    get: function() { return this._entries.length; },
    enumerable: true,
  });
  Object.defineProperty(History.prototype, 'state', {
    get: function() { return this._entries[this._index]; },
    enumerable: true,
  });
  function cloneState(state) {
    return state === undefined ? null : structuredClone(state);
  }
  History.prototype.pushState = function(state, _title, url) {
    this._entries = this._entries.slice(0, this._index + 1);
    this._entries.push(cloneState(state));
    this._index++;
    if (url !== undefined && url !== null) location.href = url;
  };
  History.prototype.replaceState = function(state, _title, url) {
    this._entries[this._index] = cloneState(state);
    if (url !== undefined && url !== null) location.href = url;
  };
  History.prototype.go = function(delta) {
    delta = delta === undefined ? 0 : Math.trunc(Number(delta) || 0);
    if (delta === 0) return;
    var next = this._index + delta;
    if (next < 0 || next > this._entries.length - 1) return;
    this._index = next;
    var ev = new Event('popstate');
    ev.state = this._entries[this._index];
    dispatchEvent(ev);
  };
  History.prototype.back = function() { this.go(-1); };
  History.prototype.forward = function() { this.go(1); };

  globalThis.History = History;
  globalThis.history = new History();
})();
)JS";

// ── window.getSelection ──────────────────────────────────────────────────
// No layout engine backs this sandbox, so there is nothing to select — this
// mirrors matchMedia's stance: an always-empty Selection rather than a
// missing global, so defensive `window.getSelection().toString()` checks in
// third-party scripts don't crash the sandbox.

const char* kSelectionBootstrapScript = R"JS(
(function() {
  function Selection() {}
  Object.defineProperty(Selection.prototype, 'rangeCount', { get: function() { return 0; }, enumerable: true });
  Object.defineProperty(Selection.prototype, 'isCollapsed', { get: function() { return true; }, enumerable: true });
  Object.defineProperty(Selection.prototype, 'anchorNode', { get: function() { return null; }, enumerable: true });
  Object.defineProperty(Selection.prototype, 'anchorOffset', { get: function() { return 0; }, enumerable: true });
  Object.defineProperty(Selection.prototype, 'focusNode', { get: function() { return null; }, enumerable: true });
  Object.defineProperty(Selection.prototype, 'focusOffset', { get: function() { return 0; }, enumerable: true });
  Object.defineProperty(Selection.prototype, 'type', { get: function() { return 'None'; }, enumerable: true });
  Selection.prototype.toString = function() { return ''; };
  Selection.prototype.removeAllRanges = function() {};
  Selection.prototype.collapse = function() {};
  Selection.prototype.collapseToStart = function() {};
  Selection.prototype.collapseToEnd = function() {};
  Selection.prototype.selectAllChildren = function() {};
  Selection.prototype.addRange = function() {};
  Selection.prototype.containsNode = function() { return false; };
  Selection.prototype.getRangeAt = function(index) {
    throw new DOMException(
      "Failed to execute 'getRangeAt' on 'Selection': " + index + ' is not a valid index.', 'IndexSizeError');
  };
  Selection.prototype.empty = Selection.prototype.removeAllRanges;

  var selectionInstance = new Selection();
  globalThis.Selection = Selection;
  globalThis.getSelection = function() { return selectionInstance; };
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

  JSValue history_result = JS_Eval(ctx, kHistoryBootstrapScript, strlen(kHistoryBootstrapScript),
                                    "<history-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(history_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, history_result);

  JSValue selection_result = JS_Eval(ctx, kSelectionBootstrapScript, strlen(kSelectionBootstrapScript),
                                      "<selection-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(selection_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, selection_result);

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

  JSValue structured_clone_result = JS_Eval(ctx, kStructuredCloneBootstrapScript, strlen(kStructuredCloneBootstrapScript),
                                             "<structured-clone-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(structured_clone_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, structured_clone_result);
}

} // namespace margelo::nitro::nitrojsdom
