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

  // ── Constraint Validation API (ValidityState, checkValidity, ...) ─────────
  // Covers the subset real-world CMS forms actually hit: required, pattern,
  // min/max/minlength/maxlength, type="email"/"url"/"number" and custom
  // validity. `step` is intentionally not modeled (stepMismatch is always
  // false) — it needs the same "default step base" arithmetic per input type
  // that browsers hardcode, which isn't worth it for this sandbox's scope.
  // reportValidity() has no UI to report against here, so it's just an alias
  // for checkValidity() (same outcome jsdom has, for the same reason).

  var VALIDITY_KEYS = [
    'valueMissing', 'typeMismatch', 'patternMismatch', 'tooLong', 'tooShort',
    'rangeUnderflow', 'rangeOverflow', 'stepMismatch', 'badInput', 'customError',
  ];

  function ValidityState(flags) {
    this._flags = flags;
  }
  VALIDITY_KEYS.forEach(function(key) {
    Object.defineProperty(ValidityState.prototype, key, {
      enumerable: true,
      get: function() { return !!this._flags[key]; },
    });
  });
  Object.defineProperty(ValidityState.prototype, 'valid', {
    enumerable: true,
    get: function() {
      var flags = this._flags;
      return !VALIDITY_KEYS.some(function(key) { return flags[key]; });
    },
  });
  globalThis.ValidityState = ValidityState;

  var EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
  var URL_RE = /^[a-zA-Z][a-zA-Z\d+\-.]*:\/\/\S+$/;

  function isFieldElement(el) {
    return el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.tagName === 'TEXTAREA';
  }

  function fieldType(el) {
    if (el.tagName === 'TEXTAREA') return 'textarea';
    if (el.tagName === 'SELECT') return 'select';
    return (el.getAttribute('type') || 'text').toLowerCase();
  }

  function computeValidityFlags(el) {
    var flags = {};
    if (el._customValidity) flags.customError = true;

    if (!isFieldElement(el) || el.hasAttribute('disabled')) return flags;

    var type = fieldType(el);
    if (type === 'hidden') return flags;

    var required = el.hasAttribute('required');

    if (type === 'checkbox' || type === 'radio') {
      if (required && !el.checked) flags.valueMissing = true;
      return flags;
    }

    var value = el.value;

    if (el.tagName === 'SELECT') {
      if (required && !value) flags.valueMissing = true;
      return flags;
    }

    if (required && value === '') flags.valueMissing = true;
    if (value === '') return flags;

    if (type === 'email' && !EMAIL_RE.test(value)) flags.typeMismatch = true;
    if (type === 'url' && !URL_RE.test(value)) flags.typeMismatch = true;

    var pattern = el.getAttribute('pattern');
    if (pattern) {
      try {
        if (!new RegExp('^(?:' + pattern + ')$').test(value)) flags.patternMismatch = true;
      } catch (e) {
        // Invalid pattern attribute: browsers treat this as never matching,
        // but silently ignoring it is the safer default for a headless sandbox.
      }
    }

    var maxlength = el.getAttribute('maxlength');
    if (maxlength !== null && value.length > parseInt(maxlength, 10)) flags.tooLong = true;

    var minlength = el.getAttribute('minlength');
    if (minlength !== null && value.length < parseInt(minlength, 10)) flags.tooShort = true;

    if (type === 'number' || type === 'range') {
      var num = Number(value);
      if (value.trim() === '' || isNaN(num)) {
        flags.badInput = true;
      } else {
        var min = el.getAttribute('min');
        var max = el.getAttribute('max');
        if (min !== null && num < Number(min)) flags.rangeUnderflow = true;
        if (max !== null && num > Number(max)) flags.rangeOverflow = true;
      }
    }

    return flags;
  }

  Object.defineProperty(Element.prototype, 'validity', {
    configurable: true,
    get: function() { return new ValidityState(computeValidityFlags(this)); },
  });

  Object.defineProperty(Element.prototype, 'willValidate', {
    configurable: true,
    get: function() {
      return isFieldElement(this) && !this.hasAttribute('disabled') && fieldType(this) !== 'hidden';
    },
  });

  var DEFAULT_MESSAGES = {
    valueMissing: 'Please fill out this field.',
    typeMismatch: 'Please enter a valid value.',
    patternMismatch: 'Please match the requested format.',
    tooLong: 'Please shorten this text.',
    tooShort: 'Please lengthen this text.',
    rangeUnderflow: 'Value must be greater than or equal to the minimum.',
    rangeOverflow: 'Value must be less than or equal to the maximum.',
    badInput: 'Please enter a valid value.',
  };

  Object.defineProperty(Element.prototype, 'validationMessage', {
    configurable: true,
    get: function() {
      if (this._customValidity) return this._customValidity;
      var flags = computeValidityFlags(this);
      for (var i = 0; i < VALIDITY_KEYS.length; i++) {
        if (flags[VALIDITY_KEYS[i]]) return DEFAULT_MESSAGES[VALIDITY_KEYS[i]] || '';
      }
      return '';
    },
  });

  Element.prototype.setCustomValidity = function(message) {
    this._customValidity = message == null ? '' : String(message);
  };

  function fieldCheckValidity(el) {
    var valid = new ValidityState(computeValidityFlags(el)).valid;
    if (!valid) {
      var evt = new Event('invalid', { bubbles: false, cancelable: true });
      el.dispatchEvent(evt);
    }
    return valid;
  }

  function formFields(form) {
    return Array.prototype.filter.call(form.querySelectorAll('input, select, textarea'), function(el) {
      return !el.hasAttribute('disabled');
    });
  }

  Element.prototype.checkValidity = function() {
    if (this.tagName === 'FORM') {
      var fields = formFields(this);
      var allValid = true;
      for (var i = 0; i < fields.length; i++) {
        if (!fieldCheckValidity(fields[i])) allValid = false;
      }
      return allValid;
    }
    return fieldCheckValidity(this);
  };

  Element.prototype.reportValidity = function() {
    return this.checkValidity();
  };

  // ── element.form / form.elements ────────────────────────────────────────
  var FORM_ASSOCIATED_TAGS = ['INPUT', 'SELECT', 'TEXTAREA', 'BUTTON', 'FIELDSET', 'OUTPUT'];

  function formControls(form) {
    return Array.prototype.slice.call(form.querySelectorAll('input, select, textarea, button, fieldset, output'));
  }

  Object.defineProperty(Element.prototype, 'form', {
    configurable: true,
    get: function() {
      if (FORM_ASSOCIATED_TAGS.indexOf(this.tagName) === -1) return undefined;
      var formId = this.getAttribute('form');
      if (formId) {
        var byId = document.getElementById(formId);
        if (byId && byId.tagName === 'FORM') return byId;
      }
      return this.closest('form');
    },
  });

  Object.defineProperty(Element.prototype, 'elements', {
    configurable: true,
    get: function() {
      if (this.tagName !== 'FORM') return undefined;
      return formControls(this);
    },
  });

  var LABELABLE_TAGS = ['BUTTON', 'INPUT', 'METER', 'OUTPUT', 'PROGRESS', 'SELECT', 'TEXTAREA'];

  Object.defineProperty(Element.prototype, 'labels', {
    configurable: true,
    get: function() {
      if (LABELABLE_TAGS.indexOf(this.tagName) === -1) return null;
      if (this.tagName === 'INPUT' && (this.getAttribute('type') || '').toLowerCase() === 'hidden') return null;
      var result = [];
      if (this.id) {
        var escapedId = this.id.replace(/"/g, '\\"');
        var byFor = document.querySelectorAll('label[for="' + escapedId + '"]');
        for (var i = 0; i < byFor.length; i++) result.push(byFor[i]);
      }
      var wrapping = this.closest('label');
      if (wrapping && result.indexOf(wrapping) === -1) result.push(wrapping);
      return result;
    },
  });
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
