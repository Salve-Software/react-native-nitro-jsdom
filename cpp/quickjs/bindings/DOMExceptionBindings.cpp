#include "DOMExceptionBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kDOMExceptionBootstrapScript = R"JS(
(function() {
  var LEGACY_CODES = {
    IndexSizeError: 1,
    HierarchyRequestError: 3,
    WrongDocumentError: 4,
    InvalidCharacterError: 5,
    NoModificationAllowedError: 7,
    NotFoundError: 8,
    NotSupportedError: 9,
    InUseAttributeError: 10,
    InvalidStateError: 11,
    SyntaxError: 12,
    InvalidModificationError: 13,
    NamespaceError: 14,
    InvalidAccessError: 15,
    TypeMismatchError: 17,
    SecurityError: 18,
    NetworkError: 19,
    AbortError: 20,
    URLMismatchError: 21,
    QuotaExceededError: 22,
    TimeoutError: 23,
    InvalidNodeTypeError: 24,
    DataCloneError: 25,
  };

  function DOMException(message, name) {
    this.message = message !== undefined ? String(message) : '';
    this.name = name !== undefined ? String(name) : 'Error';
    this.code = LEGACY_CODES[this.name] || 0;
    this.stack = new Error(this.message).stack;
  }
  DOMException.prototype = Object.create(Error.prototype);
  DOMException.prototype.constructor = DOMException;
  DOMException.prototype.toString = function() {
    return this.name + ': ' + this.message;
  };

  // Legacy static/instance numeric constants, e.g. DOMException.NOT_FOUND_ERR.
  var STATIC_CONST_NAMES = {
    IndexSizeError: 'INDEX_SIZE_ERR',
    HierarchyRequestError: 'HIERARCHY_REQUEST_ERR',
    WrongDocumentError: 'WRONG_DOCUMENT_ERR',
    InvalidCharacterError: 'INVALID_CHARACTER_ERR',
    NoModificationAllowedError: 'NO_MODIFICATION_ALLOWED_ERR',
    NotFoundError: 'NOT_FOUND_ERR',
    NotSupportedError: 'NOT_SUPPORTED_ERR',
    InUseAttributeError: 'INUSE_ATTRIBUTE_ERR',
    InvalidStateError: 'INVALID_STATE_ERR',
    SyntaxError: 'SYNTAX_ERR',
    InvalidModificationError: 'INVALID_MODIFICATION_ERR',
    NamespaceError: 'NAMESPACE_ERR',
    InvalidAccessError: 'INVALID_ACCESS_ERR',
    TypeMismatchError: 'TYPE_MISMATCH_ERR',
    SecurityError: 'SECURITY_ERR',
    NetworkError: 'NETWORK_ERR',
    InvalidNodeTypeError: 'INVALID_NODE_TYPE_ERR',
    DataCloneError: 'DATA_CLONE_ERR',
  };
  Object.keys(STATIC_CONST_NAMES).forEach(function(name) {
    var constName = STATIC_CONST_NAMES[name];
    var code = LEGACY_CODES[name];
    Object.defineProperty(DOMException, constName, { value: code, enumerable: true });
    Object.defineProperty(DOMException.prototype, constName, { value: code, enumerable: true });
  });

  globalThis.DOMException = DOMException;
})();
)JS";

} // namespace

void DOMExceptionBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kDOMExceptionBootstrapScript, strlen(kDOMExceptionBootstrapScript),
                            "<dom-exception-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

JSValue throw_dom_exception(JSContext* ctx, const char* name, const char* message) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue ctor = JS_GetPropertyStr(ctx, global, "DOMException");
  JS_FreeValue(ctx, global);

  if (!JS_IsFunction(ctx, ctor)) {
    JS_FreeValue(ctx, ctor);
    return JS_ThrowTypeError(ctx, "%s: %s", name, message);
  }

  JSValue args[2] = { JS_NewString(ctx, message), JS_NewString(ctx, name) };
  JSValue exc = JS_CallConstructor(ctx, ctor, 2, args);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, ctor);

  if (JS_IsException(exc)) return JS_EXCEPTION;
  JS_Throw(ctx, exc);
  return JS_EXCEPTION;
}

} // namespace margelo::nitro::nitrojsdom
