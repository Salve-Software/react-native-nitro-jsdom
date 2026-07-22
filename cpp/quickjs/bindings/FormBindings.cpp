#include "FormBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kFormBootstrapScript = R"JS(
(function() {
  function FormData(form) {
    this._entries = [];
    if (form) {
      var els = form.querySelectorAll('input, textarea, select');
      for (var i = 0; i < els.length; i++) {
        var el = els[i];
        var name = el.getAttribute('name');
        if (!name) continue;
        var type = (el.getAttribute('type') || '').toLowerCase();
        if ((type === 'checkbox' || type === 'radio') && !el.checked) continue;
        this._entries.push([name, el.value]);
      }
    }
  }

  FormData.prototype.append = function(name, value) {
    this._entries.push([String(name), String(value)]);
  };
  FormData.prototype.set = function(name, value) {
    name = String(name);
    value = String(value);
    var replaced = false;
    var next = [];
    for (var i = 0; i < this._entries.length; i++) {
      var entry = this._entries[i];
      if (entry[0] !== name) { next.push(entry); continue; }
      if (!replaced) { next.push([name, value]); replaced = true; }
    }
    if (!replaced) next.push([name, value]);
    this._entries = next;
  };
  FormData.prototype.get = function(name) {
    name = String(name);
    for (var i = 0; i < this._entries.length; i++) {
      if (this._entries[i][0] === name) return this._entries[i][1];
    }
    return null;
  };
  FormData.prototype.getAll = function(name) {
    name = String(name);
    var result = [];
    for (var i = 0; i < this._entries.length; i++) {
      if (this._entries[i][0] === name) result.push(this._entries[i][1]);
    }
    return result;
  };
  FormData.prototype.has = function(name) {
    name = String(name);
    for (var i = 0; i < this._entries.length; i++) {
      if (this._entries[i][0] === name) return true;
    }
    return false;
  };
  FormData.prototype.delete = function(name) {
    name = String(name);
    var next = [];
    for (var i = 0; i < this._entries.length; i++) {
      if (this._entries[i][0] !== name) next.push(this._entries[i]);
    }
    this._entries = next;
  };
  FormData.prototype.forEach = function(callback, thisArg) {
    for (var i = 0; i < this._entries.length; i++) {
      callback.call(thisArg, this._entries[i][1], this._entries[i][0], this);
    }
  };
  FormData.prototype.entries = function() {
    var entries = this._entries;
    var i = 0;
    var iter = {
      next: function() {
        if (i >= entries.length) return { done: true, value: undefined };
        return { done: false, value: entries[i++].slice() };
      },
    };
    iter[Symbol.iterator] = function() { return iter; };
    return iter;
  };
  FormData.prototype.keys = function() {
    var it = this.entries();
    var keysIter = {
      next: function() {
        var r = it.next();
        return r.done ? r : { done: false, value: r.value[0] };
      },
    };
    keysIter[Symbol.iterator] = function() { return keysIter; };
    return keysIter;
  };
  FormData.prototype.values = function() {
    var it = this.entries();
    var valuesIter = {
      next: function() {
        var r = it.next();
        return r.done ? r : { done: false, value: r.value[1] };
      },
    };
    valuesIter[Symbol.iterator] = function() { return valuesIter; };
    return valuesIter;
  };
  FormData.prototype[Symbol.iterator] = function() { return this.entries(); };

  globalThis.FormData = FormData;

  // ── form.requestSubmit() / form.submit() ──────────────────────────────────
  function isFormElement(el) {
    return !!el && el.tagName === 'FORM';
  }
  Element.prototype.requestSubmit = function(submitter) {
    if (!isFormElement(this)) return;
    var evt = new Event('submit', { bubbles: true, cancelable: true });
    evt.submitter = submitter || null;
    this.dispatchEvent(evt);
  };
  Element.prototype.submit = function() {
    // Per spec, submit() bypasses the "submit" event and constraint validation.
    // There is no real navigation in this sandbox, so this is intentionally inert.
  };
})();
)JS";

} // namespace

void FormBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kFormBootstrapScript, strlen(kFormBootstrapScript),
                            "<form-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
