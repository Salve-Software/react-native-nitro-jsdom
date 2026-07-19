---
id: roadmap
title: Roadmap
sidebar_position: 5
---

# Roadmap

## v0.1: MVP
- [x] Nitro spec + TypeScript API (`JSDOM.create`, `evaluate`, `serialize`, `dispose`)
- [x] C++ structure (`HybridHtmlSandbox`, `LexborDocument`, `QuickJSRuntime`, `DOMBindings`)
- [x] Lexbor integration: real HTML parsing and DOM queries
- [x] QuickJS integration: real JS execution
- [x] iOS support
- [x] Android support

## v0.2: Real DOM inside evaluate()
- [x] Wire real Lexbor HTML parsing so `evaluate()` sees a live DOM
- [x] Wire QuickJS JS execution so scripts run inside the sandbox
- [x] `document.querySelector(sel)` / `document.querySelectorAll(sel)` return real element objects inside `evaluate()`
- [x] `element.textContent` / `element.innerHTML` getter and setter accessible inside `evaluate()`
- [x] `document.getAttribute(sel, attr)` / `document.setAttribute(sel, attr, val)` inside `evaluate()`
- [x] `document.createElement` / `element.appendChild` / `element.removeChild` inside `evaluate()`

## v0.3: Async & Events
- [x] `addEventListener` / `removeEventListener`
- [x] `setTimeout` / `setInterval` / `clearTimeout` / `clearInterval`
- [x] `Promise` support inside sandbox (microtask drain + uncaught rejection propagation)
- [x] `console.log` forwarding to RN console via `onConsole` option
- [x] `dispatchEvent(new Event('type'))` dispatches to registered listeners
- [x] `evaluate()` drains the event loop completely before returning

## v0.4: Network & Storage
- [x] `fetch` (bridged through RN's network stack)
- [x] `localStorage` / `sessionStorage` stubs
- [x] `XMLHttpRequest` stub

## v0.5: jsdom Compatibility
- [x] Full jsdom API parity audit
- [x] `window.location`
- [x] `MutationObserver`
- [x] `window.alert` / `window.confirm` / `window.prompt`
- [x] `CustomEvent`

## v0.6: Node & Style API
> Gaps identified by the v0.5 jsdom API parity audit, prioritized by how often
> real-world HTML/JS embedded scripts hit them.
- [x] Generic Node traversal (`childNodes`, `nodeType`, `nodeName`, `nodeValue`, `firstChild` / `lastChild` / `nextSibling` / `previousSibling`, `parentNode`)
- [x] `document.getElementsByClassName` / `document.getElementsByTagName`
- [x] `element.cloneNode()`
- [x] `element.dataset` (mirrors `data-*` attributes)
- [x] `element.style` (`CSSStyleDeclaration`-like object)
- [x] `document.title`
- [x] Real event bubbling (`dispatchEvent` walks ancestors; `stopPropagation()` / `preventDefault()` take effect)

## v0.7: Node Identity & DOM Ergonomics
> Gaps identified comparing against jsdom for real-world embedded scripts:
> stable node identity plus the traversal/mutation methods those scripts
> reach for most often. Layout-dependent APIs (`getComputedStyle`,
> `getBoundingClientRect`, full CSSOM) are out of scope, since there's no
> rendering to back them.
- [x] Stable node identity (`el.firstChild === el.firstChild`) via a per-runtime wrapper cache
- [x] `node.contains(other)`
- [x] `element.closest(selector)`
- [x] `node.replaceChild(newChild, oldChild)`
- [x] `node.before(...nodes)` / `after(...nodes)` / `replaceWith(...nodes)`
- [x] `element.append(...nodes)` / `prepend(...nodes)`
- [x] `element.insertAdjacentHTML(position, html)`

## v0.8: Attribute Enumeration & Node Comparison
- [x] `document.createComment()` / `document.createDocumentFragment()`
- [x] `element.getAttributeNames()`
- [x] `element.attributes` (snapshot array of `{name, value}`)
- [x] `element.toggleAttribute(name, force?)`
- [x] `node.isSameNode(other)` / `node.isEqualNode(other)`
