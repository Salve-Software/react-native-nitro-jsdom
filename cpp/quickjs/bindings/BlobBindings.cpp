#include "BlobBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kBlobBootstrapScript = R"JS(
(function() {
  function partToBytes(part) {
    if (part instanceof ArrayBuffer) {
      return Array.prototype.slice.call(new Uint8Array(part));
    }
    if (part && typeof part.byteLength === 'number' && part.buffer !== undefined) {
      return Array.prototype.slice.call(new Uint8Array(part.buffer, part.byteOffset, part.byteLength));
    }
    if (part instanceof Blob) {
      return part._bytes.slice();
    }
    return Array.prototype.slice.call(new TextEncoder().encode(String(part)));
  }

  function Blob(parts, options) {
    var bytes = [];
    if (parts) {
      for (var i = 0; i < parts.length; i++) {
        bytes = bytes.concat(partToBytes(parts[i]));
      }
    }
    this._bytes = bytes;
    this.size = bytes.length;
    this.type = (options && options.type) ? String(options.type) : '';
  }
  Blob.prototype.slice = function(start, end, contentType) {
    var sliced = this._bytes.slice(start, end);
    var result = new Blob([]);
    result._bytes = sliced;
    result.size = sliced.length;
    result.type = contentType ? String(contentType) : '';
    return result;
  };
  Blob.prototype.text = function() {
    var bytes = this._bytes;
    var buf = new ArrayBuffer(bytes.length);
    var view = new Uint8Array(buf);
    for (var i = 0; i < bytes.length; i++) view[i] = bytes[i];
    return Promise.resolve(new TextDecoder().decode(buf));
  };
  Blob.prototype.arrayBuffer = function() {
    var bytes = this._bytes;
    var buf = new ArrayBuffer(bytes.length);
    var view = new Uint8Array(buf);
    for (var i = 0; i < bytes.length; i++) view[i] = bytes[i];
    return Promise.resolve(buf);
  };
  globalThis.Blob = Blob;

  function File(parts, name, options) {
    Blob.call(this, parts, options);
    this.name = String(name);
    if (options && options.lastModified !== undefined) {
      var n = Number(options.lastModified);
      this.lastModified = (isNaN(n) || !isFinite(n)) ? 0 : Math.trunc(n);
    } else {
      this.lastModified = Date.now();
    }
  }
  File.prototype = Object.create(Blob.prototype);
  File.prototype.constructor = File;
  globalThis.File = File;

  function FileReader() {
    this.readyState = 0;
    this.result = null;
    this.error = null;
    this.onload = null;
    this.onloadend = null;
    this.onerror = null;
    this._listeners = {};
  }
  FileReader.EMPTY = 0;
  FileReader.LOADING = 1;
  FileReader.DONE = 2;
  FileReader.prototype.addEventListener = function(type, cb) {
    if (!this._listeners[type]) this._listeners[type] = [];
    if (this._listeners[type].indexOf(cb) === -1) this._listeners[type].push(cb);
  };
  FileReader.prototype.removeEventListener = function(type, cb) {
    var list = this._listeners[type];
    if (!list) return;
    var idx = list.indexOf(cb);
    if (idx !== -1) list.splice(idx, 1);
  };
  FileReader.prototype._fire = function(type) {
    var evt = { type: type, target: this };
    var handlerProp = 'on' + type;
    if (typeof this[handlerProp] === 'function') this[handlerProp](evt);
    var list = this._listeners[type];
    if (list) list.slice().forEach(function(cb) { cb(evt); });
  };
  FileReader.prototype._read = function(blob, mode) {
    var self = this;
    self.readyState = 1;
    setTimeout(function() {
      if (!blob) {
        self.error = new Error('FileReader was given no blob to read');
        self.readyState = 2;
        self._fire('error');
        self._fire('loadend');
        return;
      }
      var bytes = blob._bytes || [];
      if (mode === 'text') {
        var buf = new ArrayBuffer(bytes.length);
        var view = new Uint8Array(buf);
        for (var i = 0; i < bytes.length; i++) view[i] = bytes[i];
        self.result = new TextDecoder().decode(buf);
      } else if (mode === 'dataurl') {
        var binary = '';
        for (var j = 0; j < bytes.length; j++) binary += String.fromCharCode(bytes[j]);
        self.result = 'data:' + (blob.type || '') + ';base64,' + btoa(binary);
      } else if (mode === 'arraybuffer') {
        var arrBuf = new ArrayBuffer(bytes.length);
        var arrView = new Uint8Array(arrBuf);
        for (var k = 0; k < bytes.length; k++) arrView[k] = bytes[k];
        self.result = arrBuf;
      }
      self.readyState = 2;
      self._fire('load');
      self._fire('loadend');
    }, 0);
  };
  FileReader.prototype.readAsText = function(blob) { this._read(blob, 'text'); };
  FileReader.prototype.readAsDataURL = function(blob) { this._read(blob, 'dataurl'); };
  FileReader.prototype.readAsArrayBuffer = function(blob) { this._read(blob, 'arraybuffer'); };
  globalThis.FileReader = FileReader;
})();
)JS";

} // namespace

void BlobBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kBlobBootstrapScript, strlen(kBlobBootstrapScript),
                            "<blob-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
