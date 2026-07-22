---
id: testing-webview
title: Testing WebView logic
sidebar_position: 5
---

# Testing WebView logic

If your app uses `react-native-webview` to run HTML + JavaScript, the hardest part to test is usually the logic inside — the scripts that query or mutate the DOM before passing a result back via `postMessage`.

Testing the WebView component itself (does `onLoad` fire? does `onMessage` receive data?) is a UI concern. But the logic embedded in `injectedJavaScript` is pure JS + DOM, and `react-native-nitro-jsdom` happens to be a good fit for running it in isolation.

## The problem with alternatives

**Jest (Node.js)** cannot run native modules at all, so any native-backed sandbox is off the table there.

**Testing a real WebView** with a native runner works, but comes with cost: each WebView instance takes 500ms–2s to load, you can only observe state through `postMessage`, and teardown between tests is fragile.

## The approach

Use `react-native-nitro-jsdom` to run the embedded JS directly, without any WebView:

```ts
import { JSDOM } from 'react-native-nitro-jsdom'

// the same HTML your WebView would receive
const dom = JSDOM.create(`
  <html>
    <body>
      <div id="price" data-original="100">100</div>
    </body>
  </html>
`)

// the same script you'd pass as injectedJavaScript
const result = await dom.evaluate(`
  const el = document.getElementById('price')
  const original = parseFloat(el.dataset.original)
  el.textContent = String((original * 0.9).toFixed(2))
  el.textContent
`)

// assert the output without a postMessage round-trip
expect(result).toBe('90.00')

dom.dispose()
```

Each test gets a clean, isolated DOM that starts and tears down in milliseconds.

## With react-native-harness

[`react-native-harness`](https://github.com/callstackincubator/react-native-harness) by Callstack runs Jest-style tests (`describe` / `it` / `expect`) in a real native environment — simulator, emulator, or physical device. That makes it compatible with Nitro Modules, including this library.

```ts
import { describe, it, expect, beforeEach } from 'react-native-harness'
import { JSDOM } from 'react-native-nitro-jsdom'

describe('price badge script', () => {
  let dom: ReturnType<typeof JSDOM.create>

  beforeEach(() => {
    dom = JSDOM.create(`
      <html>
        <body>
          <div id="price" data-original="100">100</div>
        </body>
      </html>
    `)
  })

  afterEach(() => {
    dom.dispose()
  })

  it('applies 10% discount', async () => {
    const result = await dom.evaluate(`
      const el = document.getElementById('price')
      el.textContent = String((parseFloat(el.dataset.original) * 0.9).toFixed(2))
      el.textContent
    `)
    expect(result).toBe('90.00')
  })

  it('preserves original value in data attribute', async () => {
    const result = await dom.evaluate(
      `document.getElementById('price').dataset.original`
    )
    expect(result).toBe('100')
  })
})
```

## What this covers and what it does not

This approach is well-suited for testing the JavaScript logic that runs inside a WebView: DOM queries, mutations, computed values, script side-effects.

It does not replace component-level tests. If you need to verify that `onMessage` fires, that `injectedJavaScriptBeforeContentLoaded` runs at the right time, or that the WebView mounts and renders — use `react-native-harness` with a real `WebView` component instead.
