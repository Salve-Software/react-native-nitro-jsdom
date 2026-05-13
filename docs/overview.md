# react-native-nitro-jsdom

> A headless HTML/DOM environment for React Native — powered by Nitro Modules, Lexbor, and QuickJS.

---

## The Problem

React Native has no native equivalent of [jsdom](https://github.com/jsdom/jsdom). When you need to parse an HTML document, manipulate its DOM, or run arbitrary JavaScript inside it — without rendering anything on screen — your only option today is a `WebView`, which:

- Is a **visual component**, forcing it into the React tree
- Makes **headless / logic-only use cases impossible** to isolate cleanly
- Ties your business logic to the UI layer
- Has **high overhead** — a full browser engine just to evaluate a script

A concrete real-world example: formula engines that reference fields whose values are HTML documents with embedded JavaScript. On the web these run inside an `<iframe>`. On React Native, developers are forced to use a hidden `WebView` — a fundamentally broken abstraction that drags the entire UI layer into what should be pure logic.

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

// Run arbitrary JS inside the isolated sandbox
await dom.evaluate(`
  document.getElementById('result').textContent = String(2 + 2)
`)

// Read values back
const value = dom.getTextContent('#result') // "4"

// Get the final HTML
const html = dom.serialize()

// Free native memory
dom.dispose()
```

The API is intentionally compatible with [jsdom](https://github.com/jsdom/jsdom) — code written for Node.js should migrate with minimal changes.

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

## Real-World Use Case: Formula Engine

The original motivation for this library — decoupling a formula engine from `WebView`.

```ts
import { JSDOM } from 'react-native-nitro-jsdom'

async function resolveField(fieldHtml: string, triggerScript: string): Promise<string> {
  const dom = JSDOM.create(fieldHtml)

  // Run the field's embedded trigger script
  await dom.evaluate(triggerScript)

  // Extract the computed value the script wrote to the DOM
  const result = await dom.evaluate(`window.__fieldValue`)

  dom.dispose()
  return result
}

// Formula engine calls this whenever it encounters a "dynamic field"
const value = await resolveField(campo.html, campo.triggerScript)
// → use `value` in the formula normally
```

**Before:** the formula engine had a hard dependency on a React Native `WebView` component, making it impossible to test or run outside of a React Native context.

**After:** the formula engine is pure logic — testable in Node.js, runnable in any environment, with no UI dependency.

---

## Public API

### `JSDOM.create(html, options?)`

Creates a new sandboxed DOM environment. Synchronous.

```ts
const dom = JSDOM.create('<html><body><p id="x">hello</p></body></html>', {
  runScripts: true,       // execute <script> tags — default: true
  url: 'about:blank',     // window.location.href — default: 'about:blank'
  pretendToBeVisual: false, // document.hidden = false — default: false
})
```

### `dom.evaluate(script)`

Runs arbitrary JavaScript inside the QuickJS sandbox. Returns the result as a string.

```ts
const result = await dom.evaluate(`document.querySelector('p').textContent`)
// → "hello"
```

### `dom.serialize()`

Returns the current HTML of the document, reflecting all mutations.

```ts
const html = dom.serialize()
```

### `dom.querySelector(selector)`

Returns the serialized outer HTML of the first matching element, or `null`.

```ts
dom.querySelector('#x') // "<p id=\"x\">hello</p>"
```

### `dom.querySelectorAll(selector)`

Returns serialized outer HTML for all matching elements.

```ts
dom.querySelectorAll('.item') // ["<li>A</li>", "<li>B</li>"]
```

### `dom.getAttribute(selector, attr)` / `dom.setAttribute(selector, attr, value)`

Read or write an attribute on the first matching element.

```ts
dom.getAttribute('#x', 'id')           // "x"
dom.setAttribute('#x', 'data-val', '42')
```

### `dom.getTextContent(selector)` / `dom.getInnerHTML(selector)` / `dom.setInnerHTML(selector, html)`

Read or replace text and inner HTML of the first matching element.

```ts
dom.getTextContent('#x')              // "hello"
dom.setInnerHTML('#x', '<b>world</b>')
dom.getInnerHTML('#x')                // "<b>world</b>"
```

### `dom.dispose()`

Frees all native memory (Lexbor document + QuickJS runtime). Always call this when done to avoid memory leaks.

---

## Comparison

| Feature | WebView (hidden) | react-native-nitro-jsdom |
|---|---|---|
| Requires React component | Yes | No |
| Runs headless / off-tree | No | Yes |
| Usable outside React Native | No | Yes |
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
- [ ] Lexbor integration — real HTML parsing and DOM queries
- [ ] QuickJS integration — real JS execution
- [ ] iOS support
- [ ] Android support

### v0.2 — DOM API
- [ ] `querySelector` / `querySelectorAll` via Lexbor selectors
- [ ] `getAttribute` / `setAttribute`
- [ ] `textContent` / `innerHTML`
- [ ] `createElement` / `appendChild` / `removeChild`

### v0.3 — Async & Events
- [ ] `addEventListener` / `removeEventListener`
- [ ] `setTimeout` / `setInterval` / `clearTimeout`
- [ ] `Promise` support inside sandbox
- [ ] `console.log` forwarding to RN console

### v0.4 — Network & Storage
- [ ] `fetch` (bridged through RN's network stack)
- [ ] `localStorage` / `sessionStorage` stubs
- [ ] `XMLHttpRequest` stub

### v0.5 — jsdom Compatibility
- [ ] Full jsdom API parity audit
- [ ] `window.location`
- [ ] `MutationObserver`
- [ ] `CustomEvent`

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
│   └── CMakeLists.txt               # Includes Lexbor + QuickJS (third_party/)
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
git submodule add https://github.com/lexbor/lexbor.git third_party/lexbor
git submodule add https://github.com/bellard/quickjs.git third_party/quickjs
```

Then uncomment the relevant blocks in `android/CMakeLists.txt` and `NitroJsdom.podspec`.
