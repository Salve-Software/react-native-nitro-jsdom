---
id: intro
title: Introduction
sidebar_position: 1
---

# react-native-nitro-jsdom

> A headless HTML/DOM environment for React Native, powered by Nitro Modules, Lexbor, and QuickJS.

## The Problem

React Native has no native equivalent of [jsdom](https://github.com/jsdom/jsdom). When you need to parse an HTML document, manipulate its DOM, or run arbitrary JavaScript inside it, without rendering anything on screen, your only option today is a `WebView`, which:

- Is a **visual component**, forcing it into the React tree
- Makes **headless / logic-only use cases impossible** to isolate cleanly
- Ties your business logic to the UI layer
- Has **high overhead**: a full browser engine just to evaluate a script

A concrete real-world example: apps that render CMS-driven content widgets, HTML payloads with a small embedded script that computes a value (a countdown timer, a personalized greeting, a discount badge) before it's shown. On the web that script just runs inside an `<iframe>`. On React Native, developers are forced to use a hidden `WebView`, a fundamentally broken abstraction that drags the entire UI layer into what should be pure logic.

**react-native-nitro-jsdom** solves this by providing a real, isolated HTML + JS sandbox that runs entirely off-screen and off the React tree.

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

// Mutate the DOM via evaluate(), the only DOM access path
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

`evaluate()` is the single door into the sandbox: all DOM queries and mutations are expressed as JavaScript strings passed to QuickJS, which delegates to Lexbor internally. The API is intentionally compatible with [jsdom](https://github.com/jsdom/jsdom), so code written for Node.js should migrate with minimal changes.

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
