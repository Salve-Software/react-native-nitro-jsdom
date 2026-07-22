# react-native-nitro-jsdom

> A headless HTML/DOM environment for React Native — powered by Nitro Modules, Lexbor, and QuickJS.

---

## The Problem

React Native has no native equivalent of [jsdom](https://github.com/jsdom/jsdom). When you need to parse an HTML document, manipulate its DOM, or run arbitrary JavaScript inside it — without rendering anything on screen — your only option today is a `WebView`, which:

- Is a **visual component**, forcing it into the React tree
- Makes **headless / logic-only use cases impossible** to isolate cleanly
- Ties your business logic to the UI layer
- Has **high overhead** — a full browser engine just to evaluate a script

A concrete real-world example: apps that render CMS-driven content widgets, HTML payloads with a small embedded script that computes a value (a countdown timer, a personalized greeting, a discount badge) before it's shown. On the web that script just runs inside an `<iframe>`. On React Native, developers are forced to use a hidden `WebView` — a fundamentally broken abstraction that drags the entire UI layer into what should be pure logic.

**react-native-nitro-jsdom** solves this by providing a real, isolated HTML + JS sandbox that runs entirely off-screen and off the React tree.

---

## What It Does

```ts
import { JSDOM } from 'react-native-nitro-jsdom'

const dom = JSDOM.create(`
  <html>
    <body>
      <div id="result">0</div>
    </body>
  </html>
`)

// Mutate the DOM via evaluate() — the only DOM access path
await dom.evaluate(`
  document.getElementById('result').textContent = String(2 + 2)
`)

// Read values back via evaluate()
const value = await dom.evaluate(`document.getElementById('result').textContent`)
// → "4"

// Get the final HTML
const html = dom.serialize()

// Free native memory
dom.dispose()
```

`evaluate()` is the single door into the sandbox — all DOM queries and mutations are expressed as JavaScript strings passed to QuickJS, which delegates to Lexbor internally. The DOM shape mirrors [jsdom](https://github.com/jsdom/jsdom) (`querySelector`, `textContent`, `dataset`, and more), though usage differs — everything runs through the async, string-based `evaluate()`, not jsdom's synchronous `new JSDOM()` + direct `dom.window.document` access.

---

## Architecture

```
TypeScript (consumer)
        │
        │  synchronous JSI call — no bridge, no JSON serialization
        ▼
Nitro Module (C++ binding layer)
        │
        ├──► Lexbor (C99)
        │      ├── WHATWG-compliant HTML parsing
        │      ├── In-memory DOM tree
        │      ├── querySelector / querySelectorAll
        │      ├── getAttribute / setAttribute
        │      ├── textContent / innerHTML
        │      └── serialize() → HTML string
        │
        └──► QuickJS (C)
               ├── Isolated JS runtime per instance
               ├── window / document stubs
               ├── DOM bindings → delegates to Lexbor
               ├── setTimeout / setInterval (own event loop)
               ├── console.log → forwarded to RN
               └── Executes arbitrary user scripts
```

### Why these technologies?

| Layer | Choice | Reason |
|---|---|---|
| Native binding | **Nitro Modules** | Synchronous, type-safe JSI bindings. Zero bridge overhead |
| HTML + DOM | **Lexbor** | Fastest WHATWG-compliant HTML parser in C99, zero dependencies |
| JS Engine | **QuickJS** | Built for embedding. Lightweight, fully isolated, ES2023 support |
| Glue | **C++** | Connects QuickJS ↔ Lexbor ↔ Nitro/JSI |

> **Why not reuse the Hermes instance React Native already uses?**
> The Hermes runtime is a single shared instance, not designed for multiple isolated environments. QuickJS was built specifically for embedding and isolation — each `JSDOM.create()` call gets its own independent runtime.

---

## Public API

### `JSDOM.create(html, options?)`

Creates a new sandboxed DOM environment. Synchronous.

```ts
const dom = JSDOM.create('<html><body><p id="x">hello</p></body></html>', {
  runScripts: true,       // execute <script> tags — default: true
  url: 'about:blank',     // window.location.href — default: 'about:blank'
  onConsole: (level, args) => console.log(`[sandbox ${level}]`, ...args),
  onAlert: (message) => console.log('[alert]', message),          // default: no-op
  onConfirm: (message) => true,                                    // default: false
  onPrompt: (message, defaultValue) => defaultValue ?? null,       // default: null
  onFetch: async (url, init) => {                                  // default: fetch() rejects
    const res = await fetch(url, init)
    return { status: res.status, statusText: res.statusText, headers: Object.fromEntries(res.headers), body: await res.text() }
  },
})
```

### `dom.evaluate(script)`

Runs arbitrary JavaScript inside the isolated QuickJS sandbox. This is the **only** door into the sandbox — all DOM queries and mutations must be expressed as JS strings passed to `evaluate()`, which executes them in QuickJS and delegates DOM operations to Lexbor internally.

Returns the stringified result of the last evaluated expression. Rejects if called after `dispose()`.

```ts
// Read from the DOM
const result = await dom.evaluate(`document.querySelector('p').textContent`)
// → "hello"

// Mutate the DOM
await dom.evaluate(`document.getElementById('result').textContent = String(2 + 2)`)

// Run any JS expression
const count = await dom.evaluate(`document.querySelectorAll('.item').length`)
// → "2"
```

### `dom.serialize()`

Returns the current HTML of the document, reflecting all DOM mutations made via `evaluate()`. Returns an empty string if called after `dispose()`.

```ts
const html = dom.serialize()
```

### `dom.dispose()`

Frees all native memory held by this sandbox (Lexbor document + QuickJS runtime). Calling `dispose()` multiple times is safe (idempotent). After disposing, any further `evaluate()` calls will reject with `"JSDOM instance has been disposed"`. Always call this when you are done to avoid memory leaks.

```ts
dom.dispose() // ← always pair with create()
```

---

## Comparison

| Feature | WebView (hidden) | react-native-nitro-jsdom |
|---|---|---|
| Requires React component | Yes | No |
| Runs headless / off-tree | No | Yes |
| Isolated instances | Complex | Native |
| Arbitrary JS execution | Yes | Yes |
| Full DOM API | Yes | Progressive |
| Memory control | Limited | Full (`dispose()`) |
| Performance overhead | High | Low |

---

## Roadmap

### v0.1 — MVP
- [x] Nitro spec + TypeScript API (`JSDOM.create`, `evaluate`, `serialize`, `dispose`)
- [x] C++ structure (`HybridHtmlSandbox`, `LexborDocument`, `QuickJSRuntime`, `DOMBindings`)
- [x] Lexbor integration — real HTML parsing and DOM queries
- [x] QuickJS integration — real JS execution
- [x] iOS support
- [x] Android support

### v0.2 — Real DOM inside evaluate()
- [x] Wire real Lexbor HTML parsing so `evaluate()` sees a live DOM
- [x] Wire QuickJS JS execution so scripts run inside the sandbox
- [x] `document.querySelector(sel)` / `document.querySelectorAll(sel)` return real element objects inside `evaluate()`
- [x] `element.textContent` / `element.innerHTML` getter and setter accessible inside `evaluate()`
- [x] `document.getAttribute(sel, attr)` / `document.setAttribute(sel, attr, val)` inside `evaluate()`
- [x] `document.createElement` / `element.appendChild` / `element.removeChild` inside `evaluate()`

### v0.3 — Async & Events
- [x] `addEventListener` / `removeEventListener`
- [x] `setTimeout` / `setInterval` / `clearTimeout` / `clearInterval`
- [x] `Promise` support inside sandbox (microtask drain + uncaught rejection propagation)
- [x] `console.log` forwarding to RN console via `onConsole` option
- [x] `dispatchEvent(new Event('type'))` dispatches to registered listeners
- [x] `evaluate()` drains the event loop completely before returning

### v0.4 — Network & Storage
- [x] `fetch` (bridged through RN's network stack)
- [x] `localStorage` / `sessionStorage` stubs
- [x] `XMLHttpRequest` stub

### v0.5 — jsdom Compatibility
- [x] Full jsdom API parity audit
- [x] `window.location`
- [x] `MutationObserver`
- [x] `window.alert` / `window.confirm` / `window.prompt`
- [x] `CustomEvent`

### v0.6 — Node & Style API
> Gaps identified by the v0.5 jsdom API parity audit, prioritized by how often
> real-world HTML/JS embedded scripts hit them.
- [x] Generic Node traversal (`childNodes`, `nodeType`, `nodeName`, `nodeValue`, `firstChild` / `lastChild` / `nextSibling` / `previousSibling`, `parentNode`)
- [x] `document.getElementsByClassName` / `document.getElementsByTagName`
- [x] `element.cloneNode()`
- [x] `element.dataset` (mirrors `data-*` attributes)
- [x] `element.style` (`CSSStyleDeclaration`-like object)
- [x] `document.title`
- [x] Real event bubbling (`dispatchEvent` walks ancestors; `stopPropagation()` / `preventDefault()` take effect)

### v0.7 — Node Identity & DOM Ergonomics
> Gaps identified comparing against jsdom for real-world embedded scripts:
> stable node identity plus the traversal/mutation methods those scripts
> reach for most often. Layout-dependent APIs (`getComputedStyle`,
> `getBoundingClientRect`, full CSSOM) are out of scope — there's no
> rendering to back them.
- [x] Stable node identity (`el.firstChild === el.firstChild`) via a per-runtime wrapper cache
- [x] `node.contains(other)`
- [x] `element.closest(selector)`
- [x] `node.replaceChild(newChild, oldChild)`
- [x] `node.before(...nodes)` / `after(...nodes)` / `replaceWith(...nodes)`
- [x] `element.append(...nodes)` / `prepend(...nodes)`
- [x] `element.insertAdjacentHTML(position, html)`

### v0.8 — Attribute Enumeration & Node Comparison
- [x] `document.createComment()` / `document.createDocumentFragment()`
- [x] `element.getAttributeNames()`
- [x] `element.attributes` (snapshot array of `{name, value}`)
- [x] `element.toggleAttribute(name, force?)`
- [x] `node.isSameNode(other)` / `node.isEqualNode(other)`

### v0.9 — Shadow DOM & Custom Elements
- [x] `element.attachShadow({mode})` / `element.shadowRoot`, backed by Lexbor's real `lxb_dom_shadow_root_t`
- [x] `customElements.define/get/whenDefined` with upgrade hooks (`connectedCallback`, `disconnectedCallback`, `attributeChangedCallback`)

### v0.10 — Real-World Embedded Script Gaps
> Gaps identified auditing against jsdom for the project's core use case
> (CMS-driven HTML fragments with a small embedded script: countdown timers,
> personalized greetings, discount badges), prioritized by how often that
> class of script actually reaches for them. Range/Selection, TreeWalker/
> NodeIterator, `window.history`, and cross-frame messaging (`postMessage`,
> `MessageChannel`) are deliberately excluded — they target full-page/SPA or
> multi-frame scenarios this sandbox isn't built for.
- [ ] `document.cookie` (get/set)
- [ ] `<template>` / `element.content` (DocumentFragment)
- [ ] Shadow DOM slotting (`<slot>`, `assignedNodes()`/`assignedElements()`, `slotchange`)
- [ ] `DOMContentLoaded` / `load` events on `document`/`window`
- [ ] `customElements.upgrade(root)`
- [ ] `document.forms` / `.images` / `.scripts` / `.links` collections
- [ ] `document.getElementsByName()`
- [ ] `node.normalize()`
- [ ] `node.compareDocumentPosition(other)`
- [ ] `DOMParser` / `XMLSerializer`
- [ ] `document.implementation.createHTMLDocument()`
- [ ] `document.doctype` / `DocumentType` node
- [ ] Form validity API (`ValidityState`, `checkValidity()` / `reportValidity()` / `setCustomValidity()`)

---

## Repository Structure

```
react-native-nitro-jsdom/
├── src/
│   ├── specs/
│   │   └── HtmlSandbox.nitro.ts     # Nitro interface definition
│   ├── classes/
│   │   └── JSDOM/
│   │       └── JSDOM.class.ts       # Public TypeScript API
│   └── index.ts
│
├── cpp/
│   ├── HybridHtmlSandbox.hpp/cpp    # Nitro HybridObject — orchestrates the two engines
│   ├── LexborDocument.hpp/cpp       # Lexbor DOM wrapper
│   ├── QuickJSRuntime.hpp/cpp       # QuickJS engine wrapper
│   └── DOMBindings.hpp/cpp          # QuickJS ↔ Lexbor bindings
│
├── nitrogen/generated/              # Auto-generated by `yarn codegen` — do not edit
│
├── android/
│   └── CMakeLists.txt               # Includes Lexbor + QuickJS (packages/)
│
├── ios/
│   └── NitroJsdom.podspec
│
├── scripts/
│   └── setup-clangd.sh              # Generates .clangd-headers/ for IDE support
│
├── docs/
│   └── overview.md                  # This file
│
└── example/                         # Example React Native app
```

---

## Development Setup

```bash
git clone https://github.com/Salve-Software/react-native-nitro-jsdom.git
cd react-native-nitro-jsdom

yarn install          # also runs setup-clangd.sh for IDE C++ support
yarn codegen          # run nitrogen + build TypeScript
yarn --cwd example pod  # install iOS CocoaPods
yarn example ios      # run example app on iOS simulator
```

### Adding Lexbor and QuickJS

```bash
git submodule add https://github.com/lexbor/lexbor.git packages/lexbor
git submodule add https://github.com/bellard/quickjs.git packages/quickjs
```

Then uncomment the relevant blocks in `android/CMakeLists.txt` and `NitroJsdom.podspec`.
