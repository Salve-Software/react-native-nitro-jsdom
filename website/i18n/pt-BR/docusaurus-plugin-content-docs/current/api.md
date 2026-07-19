---
id: api
title: API Pública
sidebar_position: 3
---

# API Pública

## `JSDOM.create(html, options?)`

Cria um novo ambiente de DOM em sandbox. Síncrono.

```ts
const dom = JSDOM.create('<html><body><p id="x">hello</p></body></html>', {
  runScripts: true,       // executa tags <script>, padrão: true
  url: 'about:blank',     // window.location.href, padrão: 'about:blank'
  onConsole: (level, args) => console.log(`[sandbox ${level}]`, ...args),
  onAlert: (message) => console.log('[alert]', message),          // padrão: no-op
  onConfirm: (message) => true,                                    // padrão: false
  onPrompt: (message, defaultValue) => defaultValue ?? null,       // padrão: null
  onFetch: async (url, init) => {                                  // padrão: fetch() rejeita
    const res = await fetch(url, init)
    return { status: res.status, statusText: res.statusText, headers: Object.fromEntries(res.headers), body: await res.text() }
  },
})
```

## `dom.evaluate(script)`

Executa JavaScript arbitrário dentro do sandbox isolado do QuickJS. Esta é a **única** porta de entrada para o sandbox: todas as consultas e mutações do DOM devem ser expressas como strings JS passadas para `evaluate()`, que as executa no QuickJS e delega as operações de DOM para o Lexbor internamente.

Retorna o resultado, convertido para string, da última expressão avaliada. Rejeita se chamado após `dispose()`.

```ts
// Lendo do DOM
const result = await dom.evaluate(`document.querySelector('p').textContent`)
// → "hello"

// Mutando o DOM
await dom.evaluate(`document.getElementById('result').textContent = String(2 + 2)`)

// Executando qualquer expressão JS
const count = await dom.evaluate(`document.querySelectorAll('.item').length`)
// → "2"
```

## `dom.serialize()`

Retorna o HTML atual do documento, refletindo todas as mutações de DOM feitas via `evaluate()`. Retorna uma string vazia se chamado após `dispose()`.

```ts
const html = dom.serialize()
```

## `dom.dispose()`

Libera toda a memória nativa mantida por este sandbox (documento Lexbor + runtime QuickJS). Chamar `dispose()` múltiplas vezes é seguro (idempotente). Após o dispose, qualquer chamada adicional a `evaluate()` será rejeitada com `"JSDOM instance has been disposed"`. Sempre chame este método quando terminar, para evitar vazamentos de memória.

```ts
dom.dispose() // ← sempre em par com create()
```
