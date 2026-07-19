#include "FetchBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include <cstring>
#include <optional>
#include <string>

namespace margelo::nitro::nitrojsdom {

namespace {

JSValue js_fetch_native(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || !rctx->fetch_callback) {
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "fetch is not available: no onFetch handler configured"));
    return result;
  }

  auto toStr = [&](JSValue v, bool& had_exception) -> std::string {
    JSValue s = JS_ToString(ctx, v);
    if (JS_IsException(s)) { had_exception = true; return ""; }
    const char* c = JS_ToCString(ctx, s);
    std::string out = c ? c : "";
    if (c) JS_FreeCString(ctx, c);
    JS_FreeValue(ctx, s);
    return out;
  };

  bool ex = false;
  std::string url    = argc >= 1 ? toStr(argv[0], ex) : "";
  if (ex) return JS_EXCEPTION;
  std::string method = argc >= 2 ? toStr(argv[1], ex) : "GET";
  if (ex) return JS_EXCEPTION;
  std::string headersJson = argc >= 3 ? toStr(argv[2], ex) : "{}";
  if (ex) return JS_EXCEPTION;
  std::optional<std::string> body;
  if (argc >= 4 && !JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
    body = toStr(argv[3], ex);
    if (ex) return JS_EXCEPTION;
  }

  std::string response;
  try {
    response = rctx->fetch_callback(url, method, headersJson, body);
  } catch (const std::exception& e) {
    return JS_ThrowInternalError(ctx, "fetch callback threw: %s", e.what());
  } catch (...) {
    return JS_ThrowInternalError(ctx, "fetch callback threw an unknown exception");
  }

  JSValue parsed = JS_ParseJSON(ctx, response.c_str(), response.size(), "<fetch-response>");
  if (JS_IsException(parsed)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "fetch: invalid response from native layer"));
    return result;
  }
  return parsed;
}

const char* kFetchBootstrapScript = R"JS(
(function() {
  function normalizeHeaders(h) {
    var out = {};
    if (!h) return out;
    if (Array.isArray(h)) {
      for (var i = 0; i < h.length; i++) {
        var pair = h[i];
        if (Array.isArray(pair) && pair.length >= 2)
          out[String(pair[0]).toLowerCase()] = String(pair[1]);
      }
    } else if (typeof h.entries === 'function') {
      for (var pair of h.entries()) out[String(pair[0]).toLowerCase()] = String(pair[1]);
    } else {
      for (var k in h) if (Object.prototype.hasOwnProperty.call(h, k)) out[String(k).toLowerCase()] = String(h[k]);
    }
    return out;
  }

  function Headers(init) {
    this._map = {};
    var norm = normalizeHeaders(init);
    for (var k in norm) this._map[String(k).toLowerCase()] = String(norm[k]);
  }
  Headers.prototype.get = function(name) {
    var v = this._map[String(name).toLowerCase()];
    return v === undefined ? null : v;
  };
  Headers.prototype.has = function(name) {
    return Object.prototype.hasOwnProperty.call(this._map, String(name).toLowerCase());
  };
  Headers.prototype.set = function(name, value) {
    this._map[String(name).toLowerCase()] = String(value);
  };
  Headers.prototype.forEach = function(cb) {
    for (var k in this._map) cb(this._map[k], k, this);
  };
  Headers.prototype.entries = function() {
    var self = this;
    var keys = Object.keys(this._map);
    var i = 0;
    var iter = {
      next: function() {
        if (i >= keys.length) return { done: true, value: undefined };
        var k = keys[i++];
        return { done: false, value: [k, self._map[k]] };
      }
    };
    iter[Symbol.iterator] = function() { return iter; };
    return iter;
  };
  globalThis.Headers = Headers;

  function Response(bodyText, init) {
    init = init || {};
    this._bodyText = (bodyText === undefined || bodyText === null) ? '' : bodyText;
    this.status = init.status !== undefined ? init.status : 200;
    this.statusText = init.statusText !== undefined ? init.statusText : '';
    this.ok = this.status >= 200 && this.status < 300;
    this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    this.bodyUsed = false;
  }
  Response.prototype.text = function() {
    this.bodyUsed = true;
    return Promise.resolve(this._bodyText);
  };
  Response.prototype.json = function() {
    this.bodyUsed = true;
    try {
      return Promise.resolve(JSON.parse(this._bodyText));
    } catch (e) {
      return Promise.reject(e);
    }
  };
  Response.prototype.clone = function() {
    return new Response(this._bodyText, { status: this.status, statusText: this.statusText, headers: this.headers });
  };
  globalThis.Response = Response;

  globalThis.fetch = function(input, init) {
    return new Promise(function(resolve, reject) {
      try {
        var url = typeof input === 'string' ? input : (input && input.url);
        init = init || {};
        var method = (init.method || 'GET').toUpperCase();
        var headers = normalizeHeaders(init.headers);
        var headersJson = JSON.stringify(headers);
        var body = (init.body === undefined || init.body === null) ? undefined : String(init.body);

        var raw = __nativeFetchSync(url, method, headersJson, body);
        if (raw.error) {
          reject(new TypeError(String(raw.error)));
          return;
        }

        var respHeaders = {};
        try { respHeaders = JSON.parse(raw.headersJson || '{}'); } catch (e) {}

        resolve(new Response(raw.body, {
          status: raw.status,
          statusText: raw.statusText,
          headers: respHeaders,
        }));
      } catch (e) {
        reject(e);
      }
    });
  };

  // ── XMLHttpRequest ──────────────────────────────────────────────────────
  // send() is fully synchronous: __nativeFetchSync blocks the calling thread
  // until the response arrives, so all readyState transitions and events
  // fire within the same call frame, before send() returns.

  function XMLHttpRequest() {
    this.readyState = XMLHttpRequest.UNSENT;
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this.responseURL = '';
    this._method = null;
    this._url = null;
    this._requestHeaders = {};
    this._responseHeaders = {};
    this._listeners = {};
    this.onreadystatechange = null;
    this.onload = null;
    this.onerror = null;
    this.onabort = null;
    this.onloadend = null;
  }

  XMLHttpRequest.UNSENT = 0;
  XMLHttpRequest.OPENED = 1;
  XMLHttpRequest.HEADERS_RECEIVED = 2;
  XMLHttpRequest.LOADING = 3;
  XMLHttpRequest.DONE = 4;

  XMLHttpRequest.prototype.UNSENT = 0;
  XMLHttpRequest.prototype.OPENED = 1;
  XMLHttpRequest.prototype.HEADERS_RECEIVED = 2;
  XMLHttpRequest.prototype.LOADING = 3;
  XMLHttpRequest.prototype.DONE = 4;

  Object.defineProperty(XMLHttpRequest.prototype, 'response', {
    get: function() { return this.responseText; }
  });

  XMLHttpRequest.prototype._setReadyState = function(state) {
    this.readyState = state;
    this._dispatch('readystatechange');
  };

  XMLHttpRequest.prototype._dispatch = function(type) {
    var evt = new Event(type);
    evt.target = this;
    var handlerName = 'on' + type;
    if (typeof this[handlerName] === 'function') {
      try { this[handlerName](evt); } catch (e) {}
    }
    var listeners = this._listeners[type];
    if (listeners) {
      listeners.slice().forEach(function(cb) {
        try { cb(evt); } catch (e) {}
      });
    }
  };

  XMLHttpRequest.prototype.open = function(method, url) {
    this._method = String(method || 'GET').toUpperCase();
    this._url = String(url || '');
    this._requestHeaders = {};
    this._responseHeaders = {};
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this.responseURL = '';
    this._setReadyState(XMLHttpRequest.OPENED);
  };

  XMLHttpRequest.prototype.setRequestHeader = function(name, value) {
    if (this.readyState !== XMLHttpRequest.OPENED) {
      var err = new Error('InvalidStateError: setRequestHeader() requires open() to have been called first');
      err.name = 'InvalidStateError';
      throw err;
    }
    this._requestHeaders[String(name).toLowerCase()] = String(value);
  };

  XMLHttpRequest.prototype.send = function(body) {
    if (this.readyState !== XMLHttpRequest.OPENED) {
      var err = new Error('InvalidStateError: send() requires open() to have been called first');
      err.name = 'InvalidStateError';
      throw err;
    }

    var headersJson = JSON.stringify(this._requestHeaders);
    var bodyArg = (body === undefined || body === null) ? undefined : String(body);

    var raw = __nativeFetchSync(this._url, this._method, headersJson, bodyArg);

    if (raw.error) {
      this.status = 0;
      this.statusText = '';
      this.responseText = '';
      this.responseURL = '';
      this._responseHeaders = {};
      this._setReadyState(XMLHttpRequest.DONE);
      this._dispatch('error');
      this._dispatch('loadend');
      return;
    }

    var respHeaders = {};
    try { respHeaders = JSON.parse(raw.headersJson || '{}'); } catch (e) {}
    var normalized = {};
    for (var k in respHeaders) normalized[String(k).toLowerCase()] = String(respHeaders[k]);
    this._responseHeaders = normalized;
    this.status = raw.status;
    this.statusText = raw.statusText || '';
    this.responseURL = this._url;

    this._setReadyState(XMLHttpRequest.HEADERS_RECEIVED);
    this._setReadyState(XMLHttpRequest.LOADING);
    this.responseText = raw.body || '';
    this._setReadyState(XMLHttpRequest.DONE);
    this._dispatch('load');
    this._dispatch('loadend');
  };

  XMLHttpRequest.prototype.abort = function() {
    if (this.readyState === XMLHttpRequest.UNSENT || this.readyState === XMLHttpRequest.DONE) {
      return;
    }
    this.status = 0;
    this.statusText = '';
    this.responseText = '';
    this._setReadyState(XMLHttpRequest.DONE);
    this._dispatch('abort');
    this._dispatch('loadend');
  };

  XMLHttpRequest.prototype.getAllResponseHeaders = function() {
    var lines = [];
    for (var k in this._responseHeaders) lines.push(k + ': ' + this._responseHeaders[k]);
    return lines.join('\r\n');
  };

  XMLHttpRequest.prototype.getResponseHeader = function(name) {
    var v = this._responseHeaders[String(name).toLowerCase()];
    return v === undefined ? null : v;
  };

  XMLHttpRequest.prototype.addEventListener = function(type, cb) {
    if (typeof cb !== 'function') return;
    if (!this._listeners[type]) this._listeners[type] = [];
    this._listeners[type].push(cb);
  };

  XMLHttpRequest.prototype.removeEventListener = function(type, cb) {
    var arr = this._listeners[type];
    if (!arr) return;
    var idx = arr.indexOf(cb);
    if (idx !== -1) arr.splice(idx, 1);
  };

  globalThis.XMLHttpRequest = XMLHttpRequest;
})();
)JS";

} // namespace

void FetchBindings::install(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "__nativeFetchSync", JS_NewCFunction(ctx, js_fetch_native, "__nativeFetchSync", 4));
  JS_FreeValue(ctx, global);

  JSValue bootstrap_result = JS_Eval(ctx, kFetchBootstrapScript, strlen(kFetchBootstrapScript),
                                      "<fetch-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(bootstrap_result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, bootstrap_result);
}

} // namespace margelo::nitro::nitrojsdom
