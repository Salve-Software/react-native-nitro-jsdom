#include "CSSOMBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kCSSOMBootstrapScript = R"JS(
(function() {
  function stripComments(text) {
    return text.replace(/\/\*[\s\S]*?\*\//g, '');
  }

  function camelToKebab(name) {
    return name.replace(/[A-Z]/g, function(c) { return '-' + c.toLowerCase(); });
  }

  // Splits a stylesheet's raw text into top-level rule descriptors, tracking
  // brace depth so a nested block (e.g. the rules inside @media) is captured
  // as one opaque chunk rather than mis-parsed as a sibling rule.
  function splitTopLevelRules(cssText) {
    var text = stripComments(String(cssText === undefined ? '' : cssText));
    var descriptors = [];
    var i = 0;
    var len = text.length;
    while (i < len) {
      while (i < len && /\s/.test(text.charAt(i))) i++;
      if (i >= len) break;
      var start = i;
      var depth = 0;
      var j = i;
      while (j < len) {
        var c = text.charAt(j);
        if (c === '{') {
          depth++;
        } else if (c === '}') {
          depth--;
          if (depth <= 0) { j++; break; }
        } else if (c === ';' && depth === 0) {
          j++;
          break;
        }
        j++;
      }
      var raw = text.slice(start, j);
      if (raw.trim()) descriptors.push(parseOneRule(raw));
      i = j;
    }
    return descriptors;
  }

  function parseOneRule(raw) {
    var trimmed = raw.trim();
    var braceIdx = trimmed.indexOf('{');
    if (trimmed.charAt(0) === '@' || braceIdx === -1) {
      return { kind: 'unknown', cssText: trimmed };
    }
    var selectorText = trimmed.slice(0, braceIdx).trim();
    var body = trimmed.slice(braceIdx + 1, trimmed.length - 1);
    return { kind: 'style', selectorText: selectorText, declarationText: body.trim() };
  }

  // Parses "color: red; font-size: 12px" into ordered [name, value] pairs —
  // same informal model StyleBindings.cpp uses for inline element.style.
  function parseDeclarations(text) {
    var pairs = [];
    var parts = String(text === undefined ? '' : text).split(';');
    for (var i = 0; i < parts.length; i++) {
      var part = parts[i].trim();
      if (!part) continue;
      var colonIdx = part.indexOf(':');
      if (colonIdx === -1) continue;
      var prop = part.slice(0, colonIdx).trim().toLowerCase();
      var value = part.slice(colonIdx + 1).trim();
      if (prop) pairs.push([prop, value]);
    }
    return pairs;
  }

  function makeRuleStyle(initialText) {
    var pairs = parseDeclarations(initialText);

    var target = {
      getPropertyValue: function(name) {
        var kebab = camelToKebab(String(name));
        for (var i = 0; i < pairs.length; i++) if (pairs[i][0] === kebab) return pairs[i][1];
        return '';
      },
      setProperty: function(name, value) {
        var kebab = camelToKebab(String(name));
        for (var i = 0; i < pairs.length; i++) {
          if (pairs[i][0] === kebab) { pairs[i][1] = String(value); return; }
        }
        pairs.push([kebab, String(value)]);
      },
      removeProperty: function(name) {
        var kebab = camelToKebab(String(name));
        var old = '';
        var next = [];
        for (var i = 0; i < pairs.length; i++) {
          if (pairs[i][0] === kebab && !old) { old = pairs[i][1]; continue; }
          next.push(pairs[i]);
        }
        pairs = next;
        return old;
      },
      get cssText() {
        var out = '';
        for (var i = 0; i < pairs.length; i++) out += pairs[i][0] + ': ' + pairs[i][1] + '; ';
        return out.trim();
      },
      set cssText(text) {
        pairs = parseDeclarations(text);
      },
    };

    return new Proxy(target, {
      get: function(t, prop, receiver) {
        if (typeof prop === 'string' && !(prop in t)) {
          var kebab = camelToKebab(prop);
          for (var i = 0; i < pairs.length; i++) if (pairs[i][0] === kebab) return pairs[i][1];
          return undefined;
        }
        return Reflect.get(t, prop, receiver);
      },
      set: function(t, prop, value, receiver) {
        if (typeof prop === 'string' && !(prop in t)) {
          t.setProperty(prop, value);
          return true;
        }
        return Reflect.set(t, prop, value, receiver);
      },
    });
  }

  function CSSRule() {}
  CSSRule.STYLE_RULE = 1;
  CSSRule.MEDIA_RULE = 4;
  CSSRule.prototype.type = 0;

  function CSSStyleRule(selectorText, declarationText, parentStyleSheet) {
    this.selectorText = selectorText;
    this.style = makeRuleStyle(declarationText);
    this.parentStyleSheet = parentStyleSheet || null;
    this.type = CSSRule.STYLE_RULE;
  }
  CSSStyleRule.prototype = Object.create(CSSRule.prototype);
  CSSStyleRule.prototype.constructor = CSSStyleRule;
  Object.defineProperty(CSSStyleRule.prototype, 'cssText', {
    get: function() { return this.selectorText + ' { ' + this.style.cssText + ' }'; },
    configurable: true,
  });

  function makeUnknownRule(cssText, parentStyleSheet) {
    return { type: 0, cssText: cssText, parentStyleSheet: parentStyleSheet || null };
  }

  function ruleFromDescriptor(d, sheet) {
    return d.kind === 'style'
      ? new CSSStyleRule(d.selectorText, d.declarationText, sheet)
      : makeUnknownRule(d.cssText, sheet);
  }

  function makeRuleList(rules) {
    var arr = rules.slice();
    arr.item = function(i) { return this[i] !== undefined ? this[i] : null; };
    return arr;
  }

  function CSSStyleSheet(ownerNode) {
    this.ownerNode = ownerNode || null;
    this.cssRules = makeRuleList([]);
    this.rules = this.cssRules;
  }
  CSSStyleSheet.prototype.insertRule = function(ruleText, index) {
    var idx = (index === undefined) ? this.cssRules.length : index;
    if (idx < 0 || idx > this.cssRules.length) {
      throw new DOMException(
        'The index provided (' + idx + ') is not in the allowed range.', 'IndexSizeError');
    }
    var rule = ruleFromDescriptor(parseOneRule(String(ruleText)), this);
    Array.prototype.splice.call(this.cssRules, idx, 0, rule);
    return idx;
  };
  CSSStyleSheet.prototype.deleteRule = function(index) {
    if (index < 0 || index >= this.cssRules.length) {
      throw new DOMException(
        'The index provided (' + index + ') is not in the allowed range.', 'IndexSizeError');
    }
    Array.prototype.splice.call(this.cssRules, index, 1);
  };

  function parseStylesheetInto(sheet, cssText) {
    var descriptors = splitTopLevelRules(cssText);
    for (var i = 0; i < descriptors.length; i++) {
      sheet.cssRules.push(ruleFromDescriptor(descriptors[i], sheet));
    }
  }

  globalThis.CSSRule = CSSRule;
  globalThis.CSSStyleRule = CSSStyleRule;
  globalThis.CSSStyleSheet = CSSStyleSheet;

  Object.defineProperty(Element.prototype, 'sheet', {
    get: function() {
      if (this.tagName !== 'STYLE') return null;
      if (!this._sheet) {
        var sheet = new CSSStyleSheet(this);
        parseStylesheetInto(sheet, this.textContent);
        this._sheet = sheet;
      }
      return this._sheet;
    },
    configurable: true,
  });

  Object.defineProperty(document, 'styleSheets', {
    get: function() {
      var styleEls = document.querySelectorAll('style');
      var sheets = [];
      for (var i = 0; i < styleEls.length; i++) {
        var sheet = styleEls[i].sheet;
        if (sheet) sheets.push(sheet);
      }
      sheets.item = function(i) { return this[i] !== undefined ? this[i] : null; };
      return sheets;
    },
    configurable: true,
  });
})();
)JS";

} // namespace

void CSSOMBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kCSSOMBootstrapScript, strlen(kCSSOMBootstrapScript),
                            "<cssom-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
