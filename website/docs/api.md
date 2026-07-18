---
id: api
title: Public API
sidebar_position: 3
---

# Public API

## `JSDOM.create(html, options?)`

Creates a new sandboxed DOM environment. Synchronous.

```ts
const dom = JSDOM.create('<html><body><p id="x">hello</p></body></html>', {
  runScripts: true,       // execute <script> tags, default: true
  url: 'about:blank',     // window.location.href, default: 'about:blank'
  pretendToBeVisual: false, // document.hidden = false, default: false
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

## `dom.evaluate(script)`

Runs arbitrary JavaScript inside the isolated QuickJS sandbox. This is the **only** door into the sandbox: all DOM queries and mutations must be expressed as JS strings passed to `evaluate()`, which executes them in QuickJS and delegates DOM operations to Lexbor internally.

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

## `dom.serialize()`

Returns the current HTML of the document, reflecting all DOM mutations made via `evaluate()`. Returns an empty string if called after `dispose()`.

```ts
const html = dom.serialize()
```

## `dom.dispose()`

Frees all native memory held by this sandbox (Lexbor document + QuickJS runtime). Calling `dispose()` multiple times is safe (idempotent). After disposing, any further `evaluate()` calls will reject with `"JSDOM instance has been disposed"`. Always call this when you are done to avoid memory leaks.

```ts
dom.dispose() // ← always pair with create()
```
