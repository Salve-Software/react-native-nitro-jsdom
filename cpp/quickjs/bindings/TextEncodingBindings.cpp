#include "TextEncodingBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kTextEncodingBootstrapScript = R"JS(
(function() {
  function TextEncoder() {
    this.encoding = 'utf-8';
  }
  TextEncoder.prototype.encode = function(input) {
    var str = String(input === undefined ? '' : input);
    var bytes = [];
    for (var i = 0; i < str.length; i++) {
      var code = str.charCodeAt(i);
      if (code >= 0xD800 && code <= 0xDBFF && i + 1 < str.length) {
        var next = str.charCodeAt(i + 1);
        if (next >= 0xDC00 && next <= 0xDFFF) {
          code = ((code - 0xD800) << 10) + (next - 0xDC00) + 0x10000;
          i++;
        }
      }
      if (code < 0x80) {
        bytes.push(code);
      } else if (code < 0x800) {
        bytes.push(0xC0 | (code >> 6), 0x80 | (code & 0x3F));
      } else if (code < 0x10000) {
        bytes.push(0xE0 | (code >> 12), 0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F));
      } else {
        bytes.push(
          0xF0 | (code >> 18),
          0x80 | ((code >> 12) & 0x3F),
          0x80 | ((code >> 6) & 0x3F),
          0x80 | (code & 0x3F)
        );
      }
    }
    return new Uint8Array(bytes);
  };
  TextEncoder.prototype.encodeInto = function(input, dest) {
    var encoded = this.encode(input);
    var written = Math.min(encoded.length, dest.length);
    for (var i = 0; i < written; i++) dest[i] = encoded[i];
    return { read: written === encoded.length ? String(input).length : written, written: written };
  };

  function TextDecoder(label, options) {
    this.encoding = label ? String(label).toLowerCase() : 'utf-8';
    this.fatal = !!(options && options.fatal);
    this.ignoreBOM = !!(options && options.ignoreBOM);
  }
  TextDecoder.prototype.decode = function(input) {
    var bytes;
    if (input === undefined) {
      bytes = new Uint8Array(0);
    } else if (input instanceof ArrayBuffer) {
      bytes = new Uint8Array(input);
    } else if (input && input.buffer !== undefined) {
      bytes = new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
    } else {
      bytes = new Uint8Array(0);
    }

    var result = '';
    var i = 0;
    while (i < bytes.length) {
      var b0 = bytes[i];
      var codepoint, len;
      if (b0 < 0x80) { codepoint = b0; len = 1; }
      else if ((b0 & 0xE0) === 0xC0) { codepoint = b0 & 0x1F; len = 2; }
      else if ((b0 & 0xF0) === 0xE0) { codepoint = b0 & 0x0F; len = 3; }
      else if ((b0 & 0xF8) === 0xF0) { codepoint = b0 & 0x07; len = 4; }
      else {
        if (this.fatal) throw new TypeError('The encoded data was not valid UTF-8.');
        result += '�';
        i++;
        continue;
      }
      if (i + len > bytes.length) {
        if (this.fatal) throw new TypeError('The encoded data was not valid UTF-8.');
        result += '�';
        break;
      }
      for (var k = 1; k < len; k++) {
        codepoint = (codepoint << 6) | (bytes[i + k] & 0x3F);
      }
      if (codepoint > 0xFFFF) {
        codepoint -= 0x10000;
        result += String.fromCharCode(0xD800 + (codepoint >> 10), 0xDC00 + (codepoint & 0x3FF));
      } else {
        result += String.fromCharCode(codepoint);
      }
      i += len;
    }
    if (!this.ignoreBOM && result.charCodeAt(0) === 0xFEFF) result = result.slice(1);
    return result;
  };

  globalThis.TextEncoder = TextEncoder;
  globalThis.TextDecoder = TextDecoder;
})();
)JS";

} // namespace

void TextEncodingBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kTextEncodingBootstrapScript, strlen(kTextEncodingBootstrapScript),
                            "<text-encoding-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
