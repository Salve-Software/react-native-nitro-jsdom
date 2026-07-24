#include "UrlBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kUrlBootstrapScript = R"JS(
(function() {
  function encodeComponent(str) {
    return encodeURIComponent(str).replace(/%20/g, '+').replace(/[!'()*]/g, function(c) {
      return '%' + c.charCodeAt(0).toString(16).toUpperCase();
    });
  }
  function decodeComponent(str) {
    return decodeURIComponent(str.replace(/\+/g, ' '));
  }

  function parsePairs(search) {
    var pairs = [];
    var s = search && search.charAt(0) === '?' ? search.slice(1) : (search || '');
    if (!s) return pairs;
    var parts = s.split('&');
    for (var i = 0; i < parts.length; i++) {
      var part = parts[i];
      if (!part) continue;
      var eq = part.indexOf('=');
      var k, v;
      if (eq === -1) { k = part; v = ''; } else { k = part.slice(0, eq); v = part.slice(eq + 1); }
      pairs.push([decodeComponent(k), decodeComponent(v)]);
    }
    return pairs;
  }

  function URLSearchParams(init) {
    this._pairs = [];
    this._onChange = null;
    if (init === undefined || init === null || init === '') {
    } else if (typeof init === 'string') {
      this._pairs = parsePairs(init);
    } else if (init instanceof URLSearchParams) {
      for (var i = 0; i < init._pairs.length; i++) this._pairs.push(init._pairs[i].slice());
    } else if (Array.isArray(init)) {
      for (var i = 0; i < init.length; i++) this._pairs.push([String(init[i][0]), String(init[i][1])]);
    } else if (typeof init === 'object') {
      for (var k in init) {
        if (Object.prototype.hasOwnProperty.call(init, k)) this._pairs.push([String(k), String(init[k])]);
      }
    }
  }

  URLSearchParams.prototype._notify = function() { if (this._onChange) this._onChange(); };

  URLSearchParams.prototype.append = function(name, value) {
    this._pairs.push([String(name), String(value)]);
    this._notify();
  };
  URLSearchParams.prototype.delete = function(name, value) {
    name = String(name);
    var hasValue = value !== undefined;
    var v = hasValue ? String(value) : null;
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) {
      var p = this._pairs[i];
      if (p[0] === name && (!hasValue || p[1] === v)) continue;
      out.push(p);
    }
    this._pairs = out;
    this._notify();
  };
  URLSearchParams.prototype.get = function(name) {
    name = String(name);
    for (var i = 0; i < this._pairs.length; i++) if (this._pairs[i][0] === name) return this._pairs[i][1];
    return null;
  };
  URLSearchParams.prototype.getAll = function(name) {
    name = String(name);
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) if (this._pairs[i][0] === name) out.push(this._pairs[i][1]);
    return out;
  };
  URLSearchParams.prototype.has = function(name, value) {
    name = String(name);
    var hasValue = value !== undefined;
    var v = hasValue ? String(value) : null;
    for (var i = 0; i < this._pairs.length; i++) {
      if (this._pairs[i][0] === name && (!hasValue || this._pairs[i][1] === v)) return true;
    }
    return false;
  };
  URLSearchParams.prototype.set = function(name, value) {
    name = String(name); value = String(value);
    var found = false;
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) {
      var p = this._pairs[i];
      if (p[0] !== name) { out.push(p); continue; }
      if (!found) { out.push([name, value]); found = true; }
    }
    if (!found) out.push([name, value]);
    this._pairs = out;
    this._notify();
  };
  URLSearchParams.prototype.sort = function() {
    this._pairs.sort(function(a, b) { return a[0] < b[0] ? -1 : (a[0] > b[0] ? 1 : 0); });
    this._notify();
  };
  URLSearchParams.prototype.forEach = function(cb, thisArg) {
    for (var i = 0; i < this._pairs.length; i++) cb.call(thisArg, this._pairs[i][1], this._pairs[i][0], this);
  };
  URLSearchParams.prototype.keys = function() {
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) out.push(this._pairs[i][0]);
    return out[Symbol.iterator]();
  };
  URLSearchParams.prototype.values = function() {
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) out.push(this._pairs[i][1]);
    return out[Symbol.iterator]();
  };
  URLSearchParams.prototype.entries = function() {
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) out.push(this._pairs[i].slice());
    return out[Symbol.iterator]();
  };
  URLSearchParams.prototype[Symbol.iterator] = function() { return this.entries(); };
  URLSearchParams.prototype.toString = function() {
    var out = [];
    for (var i = 0; i < this._pairs.length; i++) out.push(encodeComponent(this._pairs[i][0]) + '=' + encodeComponent(this._pairs[i][1]));
    return out.join('&');
  };

  globalThis.URLSearchParams = URLSearchParams;

  function parseUrlFull(str) {
    str = String(str === undefined || str === null ? '' : str);
    var m = /^([^:\/?#]+:)(?:\/\/(?:([^\/?#@]*)@)?([^\/?#:]*)(?::(\d+))?)?([^?#]*)(\?[^#]*)?(#.*)?$/.exec(str);
    if (!m) return null;
    var protocol = m[1] || '';
    var userinfo = m[2] || '';
    var hostname = m[3] || '';
    var port = m[4] || '';
    var pathname = m[5] || '';
    var search = m[6] || '';
    var hash = m[7] || '';
    if (!pathname && hostname) pathname = '/';
    var username = '', password = '';
    if (userinfo) {
      var idx = userinfo.indexOf(':');
      if (idx === -1) { username = userinfo; } else { username = userinfo.slice(0, idx); password = userinfo.slice(idx + 1); }
    }
    return {
      protocol: protocol, username: username, password: password,
      hostname: hostname, port: port, pathname: pathname, search: search, hash: hash,
    };
  }

  function serializeUrl(p) {
    var auth = '';
    if (p.username || p.password) auth = p.username + (p.password ? ':' + p.password : '') + '@';
    var host = p.hostname ? ('//' + auth + p.hostname + (p.port ? ':' + p.port : '')) : '';
    return (p.protocol || '') + host + (p.pathname || '') + (p.search || '') + (p.hash || '');
  }

  function resolveUrlFull(base, next) {
    next = String(next);
    if (/^[a-zA-Z][a-zA-Z0-9+\-.]*:/.test(next)) return next;
    var p = parseUrlFull(base) || {};
    var origin = p.hostname
      ? serializeUrl({ protocol: p.protocol, username: p.username, password: p.password, hostname: p.hostname, port: p.port, pathname: '', search: '', hash: '' })
      : (p.protocol || '');
    if (next.charAt(0) === '/') return origin + next;
    if (next.charAt(0) === '#') return base.split('#')[0] + next;
    if (next.charAt(0) === '?') return base.split('?')[0].split('#')[0] + next;
    var dir = (p.pathname || '/').replace(/\/[^\/]*$/, '/');
    return origin + dir + next;
  }

  function URL(url, base) {
    var href = (base !== undefined && base !== null) ? resolveUrlFull(String(base), url) : String(url);
    if (!/^[a-zA-Z][a-zA-Z0-9+\-.]*:/.test(href)) {
      throw new TypeError('Invalid URL: ' + href);
    }
    this._href = href;
    var self = this;
    var initialSearch = (parseUrlFull(this._href) || {}).search || '';
    this._searchParams = new URLSearchParams(initialSearch);
    this._searchParams._onChange = function() { self._syncSearchFromParams(); };
  }

  URL.prototype._syncSearchFromParams = function() {
    var s = this._searchParams.toString();
    this._setPart('search', s ? '?' + s : '');
  };

  URL.prototype._setPart = function(part, value) {
    var p = parseUrlFull(this._href) || {};
    p[part] = value;
    this._href = serializeUrl(p);
  };

  URL.prototype._resyncSearchParams = function() {
    var s = (parseUrlFull(this._href) || {}).search || '';
    this._searchParams._pairs = parsePairs(s);
  };

  ['protocol', 'username', 'password', 'hostname', 'port', 'pathname', 'hash'].forEach(function(name) {
    Object.defineProperty(URL.prototype, name, {
      get: function() { return (parseUrlFull(this._href) || {})[name] || ''; },
      set: function(v) { this._setPart(name, String(v)); },
      enumerable: true,
      configurable: true,
    });
  });

  Object.defineProperty(URL.prototype, 'search', {
    get: function() { return (parseUrlFull(this._href) || {}).search || ''; },
    set: function(v) {
      var s = String(v);
      this._setPart('search', s ? (s.charAt(0) === '?' ? s : '?' + s) : '');
      this._resyncSearchParams();
    },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(URL.prototype, 'searchParams', {
    get: function() { return this._searchParams; },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(URL.prototype, 'host', {
    get: function() {
      var p = parseUrlFull(this._href) || {};
      return p.hostname + (p.port ? ':' + p.port : '');
    },
    set: function(v) {
      var s = String(v);
      var idx = s.indexOf(':');
      if (idx === -1) { this._setPart('hostname', s); this._setPart('port', ''); }
      else { this._setPart('hostname', s.slice(0, idx)); this._setPart('port', s.slice(idx + 1)); }
    },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(URL.prototype, 'origin', {
    get: function() {
      var p = parseUrlFull(this._href) || {};
      if (!p.hostname) return 'null';
      return p.protocol + '//' + p.hostname + (p.port ? ':' + p.port : '');
    },
    enumerable: true,
    configurable: true,
  });

  Object.defineProperty(URL.prototype, 'href', {
    get: function() { return this._href; },
    set: function(v) {
      this._href = String(v);
      this._resyncSearchParams();
    },
    enumerable: true,
    configurable: true,
  });

  URL.prototype.toString = function() { return this._href; };
  URL.prototype.toJSON = function() { return this._href; };

  URL.canParse = function(url, base) {
    try {
      new URL(url, base);
      return true;
    } catch (e) {
      return false;
    }
  };

  globalThis.URL = URL;

  function isAnchorElement(el) {
    return !!el && (el.tagName === 'A' || el.tagName === 'AREA');
  }

  function anchorUrl(el) {
    var raw = el.getAttribute('href');
    if (raw === null) return null;
    try { return new URL(raw, document.baseURI); } catch (e) { return null; }
  }

  Object.defineProperty(Element.prototype, 'href', {
    configurable: true,
    get: function() {
      if (!isAnchorElement(this)) return undefined;
      var u = anchorUrl(this);
      return u ? u.href : '';
    },
    set: function(value) {
      if (!isAnchorElement(this)) return;
      this.setAttribute('href', String(value));
    },
  });

  ['protocol', 'username', 'password', 'hostname', 'port', 'pathname', 'search', 'hash', 'host', 'origin'].forEach(function(part) {
    Object.defineProperty(Element.prototype, part, {
      configurable: true,
      get: function() {
        if (!isAnchorElement(this)) return undefined;
        var u = anchorUrl(this);
        return u ? u[part] : '';
      },
    });
  });
})();
)JS";

} // namespace

void UrlBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kUrlBootstrapScript, strlen(kUrlBootstrapScript),
                            "<url-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
