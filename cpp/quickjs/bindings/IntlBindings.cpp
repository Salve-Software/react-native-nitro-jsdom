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

  function pluralCategoryFor(n, lang) {
    n = Math.abs(Number(n));
    if (lang === 'pt') return (n === 0 || n === 1) ? 'one' : 'other';
    return n === 1 ? 'one' : 'other';
  }

  function PluralRules(locales, options) {
    this._lang = localeLanguage(locales);
    options = options || {};
    this._type = options.type || 'cardinal';
  }

  PluralRules.prototype.select = function(n) {
    return pluralCategoryFor(n, this._lang);
  };

  PluralRules.prototype.resolvedOptions = function() {
    return { locale: this._lang, type: this._type, pluralCategories: ['one', 'other'] };
  };

  var RTF_UNITS = {
    en: {
      year: ['year', 'years'], quarter: ['quarter', 'quarters'], month: ['month', 'months'],
      week: ['week', 'weeks'], day: ['day', 'days'], hour: ['hour', 'hours'],
      minute: ['minute', 'minutes'], second: ['second', 'seconds'],
    },
    pt: {
      year: ['ano', 'anos'], quarter: ['trimestre', 'trimestres'], month: ['mês', 'meses'],
      week: ['semana', 'semanas'], day: ['dia', 'dias'], hour: ['hora', 'horas'],
      minute: ['minuto', 'minutos'], second: ['segundo', 'segundos'],
    },
  };

  var RTF_PATTERNS = {
    en: { past: '{0} {1} ago', future: 'in {0} {1}' },
    pt: { past: 'há {0} {1}', future: 'em {0} {1}' },
  };

  var RTF_AUTO = {
    en: {
      day: { '-1': 'yesterday', '0': 'today', '1': 'tomorrow' },
      week: { '-1': 'last week', '0': 'this week', '1': 'next week' },
      month: { '-1': 'last month', '0': 'this month', '1': 'next month' },
      year: { '-1': 'last year', '0': 'this year', '1': 'next year' },
      second: { '0': 'now' },
    },
    pt: {
      day: { '-1': 'ontem', '0': 'hoje', '1': 'amanhã' },
      week: { '-1': 'semana passada', '0': 'esta semana', '1': 'semana que vem' },
      month: { '-1': 'mês passado', '0': 'este mês', '1': 'próximo mês' },
      year: { '-1': 'ano passado', '0': 'este ano', '1': 'próximo ano' },
      second: { '0': 'agora' },
    },
  };

  var RTF_UNIT_ALIASES = {
    years: 'year', quarters: 'quarter', months: 'month', weeks: 'week',
    days: 'day', hours: 'hour', minutes: 'minute', seconds: 'second',
  };

  function RelativeTimeFormat(locales, options) {
    this._lang = RTF_UNITS[localeLanguage(locales)] ? localeLanguage(locales) : 'en';
    options = options || {};
    this._numeric = options.numeric || 'always';
    this._style = options.style || 'long';
  }

  RelativeTimeFormat.prototype.format = function(value, unit) {
    value = Number(value);
    unit = RTF_UNIT_ALIASES[unit] || unit;
    var units = RTF_UNITS[this._lang][unit];
    if (!units) throw new RangeError('Invalid unit argument for RelativeTimeFormat.format()');

    if (this._numeric === 'auto') {
      var autoData = RTF_AUTO[this._lang][unit];
      var rounded = Math.round(value);
      if (autoData && autoData[String(rounded)] !== undefined) {
        return autoData[String(rounded)];
      }
    }

    var n = Math.abs(value);
    var unitName = units[pluralCategoryFor(n, this._lang) === 'one' ? 0 : 1];
    var pattern = value < 0 ? RTF_PATTERNS[this._lang].past : RTF_PATTERNS[this._lang].future;
    return pattern.replace('{0}', String(n)).replace('{1}', unitName);
  };

  RelativeTimeFormat.prototype.resolvedOptions = function() {
    return { locale: this._lang, style: this._style, numeric: this._numeric };
  };

  globalThis.Intl = globalThis.Intl || {};
  globalThis.Intl.NumberFormat = NumberFormat;
  globalThis.Intl.DateTimeFormat = DateTimeFormat;
  globalThis.Intl.PluralRules = PluralRules;
  globalThis.Intl.RelativeTimeFormat = RelativeTimeFormat;

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
