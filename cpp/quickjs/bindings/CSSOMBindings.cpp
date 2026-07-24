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

  function cssEscape(value) {
    var string = String(value);
    var length = string.length;
    var index = -1;
    var result = '';
    var firstCodeUnit = string.charCodeAt(0);

    if (length === 1 && firstCodeUnit === 0x002D) {
      return '\\' + string;
    }

    while (++index < length) {
      var codeUnit = string.charCodeAt(index);
      if (codeUnit === 0x0000) {
        result += '\uFFFD';
        continue;
      }
      if (
        (codeUnit >= 0x0001 && codeUnit <= 0x001F) || codeUnit === 0x007F ||
        (index === 0 && codeUnit >= 0x0030 && codeUnit <= 0x0039) ||
        (index === 1 && codeUnit >= 0x0030 && codeUnit <= 0x0039 && firstCodeUnit === 0x002D)
      ) {
        result += '\\' + codeUnit.toString(16) + ' ';
        continue;
      }
      if (
        codeUnit >= 0x0080 || codeUnit === 0x002D || codeUnit === 0x005F ||
        (codeUnit >= 0x0030 && codeUnit <= 0x0039) ||
        (codeUnit >= 0x0041 && codeUnit <= 0x005A) ||
        (codeUnit >= 0x0061 && codeUnit <= 0x007A)
      ) {
        result += string.charAt(index);
        continue;
      }
      result += '\\' + string.charAt(index);
    }
    return result;
  }

  globalThis.CSS = {
    escape: cssEscape,
    supports: function(propertyOrCondition, value) {
      if (arguments.length >= 2) {
        return typeof propertyOrCondition === 'string' && propertyOrCondition.length > 0 &&
          value !== undefined && String(value).length > 0;
      }
      return typeof propertyOrCondition === 'string' && propertyOrCondition.trim().length > 0;
    },
  };

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

  // ── getComputedStyle ──────────────────────────────────────────────────
  // No layout/cascade engine backs this sandbox — there is no stylesheet
  // specificity resolution or inheritance, only the element's own inline
  // `style` attribute. `display` falls back to a small user-agent-stylesheet
  // approximation (block/inline/inline-block/none by tag name, or 'none' for
  // a `hidden` attribute) since "is this element visible" is the one
  // getComputedStyle check real-world embedded scripts actually make;
  // `visibility`/`opacity` fall back to their initial values. Every other
  // unset property returns '', not a computed initial value.
  var kBlockTags = { ADDRESS: 1, ARTICLE: 1, ASIDE: 1, BLOCKQUOTE: 1, DETAILS: 1, DIALOG: 1, DD: 1, DIV: 1,
    DL: 1, DT: 1, FIELDSET: 1, FIGCAPTION: 1, FIGURE: 1, FOOTER: 1, FORM: 1, H1: 1, H2: 1, H3: 1, H4: 1,
    H5: 1, H6: 1, HEADER: 1, HGROUP: 1, HR: 1, LI: 1, MAIN: 1, NAV: 1, OL: 1, P: 1, PRE: 1, SECTION: 1,
    TABLE: 1, UL: 1, HTML: 1, BODY: 1 };
  var kNoneTags = { SCRIPT: 1, STYLE: 1, HEAD: 1, TITLE: 1, META: 1, LINK: 1, TEMPLATE: 1 };
  var kInlineBlockTags = { IMG: 1, BUTTON: 1, INPUT: 1, SELECT: 1, TEXTAREA: 1 };

  function defaultDisplay(tagName) {
    if (kNoneTags[tagName]) return 'none';
    if (kInlineBlockTags[tagName]) return 'inline-block';
    if (kBlockTags[tagName]) return 'block';
    return 'inline';
  }

  function resolveProperty(el, kebabName) {
    var v = el.style.getPropertyValue(kebabName);
    if (v) return v;
    if (kebabName === 'display') return el.hasAttribute('hidden') ? 'none' : defaultDisplay(el.tagName);
    if (kebabName === 'visibility') return 'visible';
    if (kebabName === 'opacity') return '1';
    return '';
  }

  globalThis.getComputedStyle = function(el) {
    if (!el || typeof el.tagName !== 'string') {
      throw new TypeError("Failed to execute 'getComputedStyle': parameter 1 is not of type 'Element'.");
    }
    return new Proxy({}, {
      get: function(_target, prop) {
        if (prop === 'getPropertyValue') return function(name) { return resolveProperty(el, String(name)); };
        if (prop === 'getPropertyPriority') return function() { return ''; };
        if (typeof prop !== 'string') return undefined;
        return resolveProperty(el, camelToKebab(prop));
      },
    });
  };
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
