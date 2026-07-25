#include "IntlBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kIntlBootstrapScript = R"JS(
(function() {
  var LOCALE_DATA = {
    en: {
      months: {
        long: ['January','February','March','April','May','June','July','August','September','October','November','December'],
        short: ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],
        narrow: ['J','F','M','A','M','J','J','A','S','O','N','D'],
      },
      weekdays: {
        long: ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'],
        short: ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'],
        narrow: ['S','M','T','W','T','F','S'],
      },
      dayPeriod: ['AM', 'PM'],
      decimal: '.',
      group: ',',
      dateOrder: 'MDY',
      currencySpace: false,
    },
    pt: {
      months: {
        long: ['janeiro','fevereiro','março','abril','maio','junho','julho','agosto','setembro','outubro','novembro','dezembro'],
        short: ['jan','fev','mar','abr','mai','jun','jul','ago','set','out','nov','dez'],
        narrow: ['J','F','M','A','M','J','J','A','S','O','N','D'],
      },
      weekdays: {
        long: ['domingo','segunda-feira','terça-feira','quarta-feira','quinta-feira','sexta-feira','sábado'],
        short: ['dom','seg','ter','qua','qui','sex','sáb'],
        narrow: ['D','S','T','Q','Q','S','S'],
      },
      dayPeriod: ['AM', 'PM'],
      decimal: ',',
      group: '.',
      dateOrder: 'DMY',
      currencySpace: true,
    },
  };

  var CURRENCY_SYMBOLS = {
    USD: '$', EUR: '€', GBP: '£', JPY: '¥', BRL: 'R$', CAD: 'CA$', AUD: 'A$', CHF: 'CHF', CNY: 'CN¥', INR: '₹', MXN: 'MX$',
  };

  var CURRENCY_DECIMALS = {
    JPY: 0, KRW: 0, VND: 0, CLP: 0, ISK: 0, UGX: 0, XAF: 0, XOF: 0, XPF: 0,
    BHD: 3, IQD: 3, JOD: 3, KWD: 3, OMR: 3, TND: 3,
  };

  function currencyDecimals(code) {
    return CURRENCY_DECIMALS[code] !== undefined ? CURRENCY_DECIMALS[code] : 2;
  }

  function localeLanguage(locales) {
    var tag = Array.isArray(locales) ? locales[0] : locales;
    tag = String(tag || 'en');
    return tag.split(/[-_]/)[0].toLowerCase();
  }

  function resolveLocale(locales) {
    var lang = localeLanguage(locales);
    return LOCALE_DATA[lang] || LOCALE_DATA.en;
  }

  function groupInteger(digits, groupSep) {
    var out = '';
    var count = 0;
    for (var i = digits.length - 1; i >= 0; i--) {
      out = digits.charAt(i) + out;
      count++;
      if (count % 3 === 0 && i !== 0) out = groupSep + out;
    }
    return out;
  }

  function formatFixed(value, minFrac, maxFrac, useGrouping, data) {
    var negative = value < 0;
    var abs = Math.abs(value);
    var fixed = abs.toFixed(maxFrac);
    var parts = fixed.split('.');
    var intPart = parts[0];
    var fracPart = parts[1] || '';

    while (fracPart.length > minFrac && fracPart.charAt(fracPart.length - 1) === '0') {
      fracPart = fracPart.slice(0, -1);
    }

    var out = useGrouping ? groupInteger(intPart, data.group) : intPart;
    if (fracPart.length > 0) out += data.decimal + fracPart;
    return (negative ? '-' : '') + out;
  }

  function NumberFormat(locales, options) {
    this._data = resolveLocale(locales);
    options = options || {};
    this._style = options.style || 'decimal';
    if (this._style === 'currency') {
      if (!options.currency) throw new TypeError('Currency code is required with currency style.');
      this._currency = options.currency;
    }
    var currDecimals = this._style === 'currency' ? currencyDecimals(this._currency) : undefined;
    var defaultMinFrac = this._style === 'currency' ? currDecimals : 0;
    var defaultMaxFrac = this._style === 'currency' ? currDecimals : (this._style === 'percent' ? 0 : 3);
    this._minFrac = options.minimumFractionDigits !== undefined ? options.minimumFractionDigits : defaultMinFrac;
    this._maxFrac = options.maximumFractionDigits !== undefined ? options.maximumFractionDigits : Math.max(defaultMaxFrac, this._minFrac);
    this._useGrouping = options.useGrouping !== undefined ? !!options.useGrouping : true;
  }

  NumberFormat.prototype.format = function(value) {
    value = Number(value);
    if (this._style === 'percent') value = value * 100;
    var formatted = formatFixed(value, this._minFrac, this._maxFrac, this._useGrouping, this._data);
    if (this._style === 'percent') return formatted + '%';
    if (this._style === 'currency') {
      var symbol = CURRENCY_SYMBOLS[this._currency] || this._currency;
      return symbol + (this._data.currencySpace ? ' ' : '') + formatted;
    }
    return formatted;
  };

  NumberFormat.prototype.resolvedOptions = function() {
    return {
      style: this._style,
      currency: this._style === 'currency' ? this._currency : undefined,
      minimumFractionDigits: this._minFrac,
      maximumFractionDigits: this._maxFrac,
      useGrouping: this._useGrouping,
    };
  };

  function pad2(n) { return (n < 10 ? '0' : '') + n; }

  var DATE_STYLE_OPTIONS = {
    short: { year: '2-digit', month: 'numeric', day: 'numeric' },
    medium: { year: 'numeric', month: 'short', day: 'numeric' },
    long: { year: 'numeric', month: 'long', day: 'numeric' },
    full: { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' },
  };
  var TIME_STYLE_OPTIONS = {
    short: { hour: 'numeric', minute: 'numeric' },
    medium: { hour: 'numeric', minute: 'numeric', second: 'numeric' },
    long: { hour: 'numeric', minute: 'numeric', second: 'numeric' },
    full: { hour: 'numeric', minute: 'numeric', second: 'numeric' },
  };

  function DateTimeFormat(locales, options) {
    this._data = resolveLocale(locales);
    options = options || {};

    if (options.dateStyle || options.timeStyle) {
      var withDate = options.dateStyle ? (DATE_STYLE_OPTIONS[options.dateStyle] || DATE_STYLE_OPTIONS.medium) : {};
      var withTime = options.timeStyle ? (TIME_STYLE_OPTIONS[options.timeStyle] || TIME_STYLE_OPTIONS.short) : {};
      options = Object.assign({}, withDate, withTime, options);
    } else if (Object.keys(options).length === 0) {
      options = { year: 'numeric', month: 'numeric', day: 'numeric' };
    }

    this._options = options;
  }

  function datePartFor(date, o, data) {
    var textual = o.month === 'long' || o.month === 'short' || o.month === 'narrow';
    var day = o.day ? (o.day === '2-digit' ? pad2(date.getDate()) : String(date.getDate())) : '';
    var year = o.year ? (o.year === '2-digit' ? pad2(date.getFullYear() % 100) : String(date.getFullYear())) : '';
    var month = o.month
      ? (textual ? data.months[o.month][date.getMonth()] : (o.month === '2-digit' ? pad2(date.getMonth() + 1) : String(date.getMonth() + 1)))
      : '';

    if (!month && !day && !year) return '';

    if (textual) {
      if (data.dateOrder === 'MDY') {
        var head = day ? month + ' ' + day : month;
        return year ? head + ', ' + year : head;
      }
      return [day, month, year].filter(Boolean).join(' ');
    }

    var ordered = data.dateOrder === 'MDY' ? [month, day, year] : [day, month, year];
    return ordered.filter(Boolean).join('/');
  }

  DateTimeFormat.prototype.format = function(date) {
    date = date === undefined ? new Date() : (date instanceof Date ? date : new Date(date));
    var o = this._options;
    var data = this._data;
    var pieces = [];

    var datePart = datePartFor(date, o, data);
    if (o.weekday) {
      var w = date.getDay();
      var weekdayName = data.weekdays[o.weekday === 'narrow' ? 'narrow' : (o.weekday === 'long' ? 'long' : 'short')][w];
      pieces.push(datePart ? weekdayName + ', ' + datePart : weekdayName);
    } else if (datePart) {
      pieces.push(datePart);
    }

    if (o.hour) {
      var h = date.getHours();
      var hour12 = o.hour12 !== false;
      var hourVal = hour12 ? (h % 12 === 0 ? 12 : h % 12) : h;
      var timeSegs = [o.hour === '2-digit' ? pad2(hourVal) : String(hourVal)];
      if (o.minute) timeSegs.push(pad2(date.getMinutes()));
      if (o.second) timeSegs.push(pad2(date.getSeconds()));
      var timeStr = timeSegs.join(':');
      if (hour12) timeStr += ' ' + (h < 12 ? data.dayPeriod[0] : data.dayPeriod[1]);
      pieces.push(timeStr);
    } else if (o.minute) {
      var timeSegs2 = [pad2(date.getMinutes())];
      if (o.second) timeSegs2.push(pad2(date.getSeconds()));
      pieces.push(timeSegs2.join(':'));
    }

    return pieces.join(', ');
  };

  DateTimeFormat.prototype.resolvedOptions = function() {
    return Object.assign({}, this._options);
  };

  globalThis.Intl = globalThis.Intl || {};
  globalThis.Intl.NumberFormat = NumberFormat;
  globalThis.Intl.DateTimeFormat = DateTimeFormat;

  function hasOwnKeys(o) { return !!o && Object.keys(o).length > 0; }

  Number.prototype.toLocaleString = function(locales, options) {
    return new NumberFormat(locales, options).format(this);
  };

  Date.prototype.toLocaleString = function(locales, options) {
    return new DateTimeFormat(locales, hasOwnKeys(options) ? options : { year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' }).format(this);
  };
  Date.prototype.toLocaleDateString = function(locales, options) {
    return new DateTimeFormat(locales, hasOwnKeys(options) ? options : { year: 'numeric', month: 'numeric', day: 'numeric' }).format(this);
  };
  Date.prototype.toLocaleTimeString = function(locales, options) {
    return new DateTimeFormat(locales, hasOwnKeys(options) ? options : { hour: 'numeric', minute: 'numeric', second: 'numeric' }).format(this);
  };
})();
)JS";

} // namespace

void IntlBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kIntlBootstrapScript, strlen(kIntlBootstrapScript),
                            "<intl-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
