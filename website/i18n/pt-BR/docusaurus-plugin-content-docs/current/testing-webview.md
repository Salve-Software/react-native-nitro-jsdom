---
id: testing-webview
title: Testando lógica de WebView
sidebar_position: 5
---

# Testando lógica de WebView

Se o seu app usa `react-native-webview` para executar HTML + JavaScript, a parte mais difícil de testar costuma ser a lógica interna: os scripts que consultam ou mutam o DOM antes de retornar um resultado via `postMessage`.

Testar o componente WebView em si (o `onLoad` dispara? o `onMessage` recebe os dados?) é uma questão de UI. Mas a lógica embutida no `injectedJavaScript` é JS + DOM puro, e o `react-native-nitro-jsdom` acaba sendo uma boa opção para executá-la de forma isolada.

## O problema com as alternativas

**Jest (Node.js)** não consegue rodar módulos nativos, então qualquer sandbox nativo está fora de cogitação.

**Testar um WebView real** com um runner nativo funciona, mas tem um custo: cada instância de WebView leva de 500ms a 2s para carregar, você só consegue observar o estado via `postMessage`, e a limpeza entre testes é frágil.

## A abordagem

Use `react-native-nitro-jsdom` para executar o JS embutido diretamente, sem nenhum WebView:

```ts
import { JSDOM } from 'react-native-nitro-jsdom'

// o mesmo HTML que sua WebView receberia
const dom = JSDOM.create(`
  <html>
    <body>
      <div id="price" data-original="100">100</div>
    </body>
  </html>
`)

// o mesmo script que você passaria como injectedJavaScript
const result = await dom.evaluate(`
  const el = document.getElementById('price')
  const original = parseFloat(el.dataset.original)
  el.textContent = String((original * 0.9).toFixed(2))
  el.textContent
`)

// assertiva direta, sem precisar de um round-trip via postMessage
expect(result).toBe('90.00')

dom.dispose()
```

Cada teste recebe um DOM limpo e isolado que inicializa e encerra em milissegundos.

## Com react-native-harness

O [`react-native-harness`](https://github.com/callstackincubator/react-native-harness) da Callstack executa testes no estilo Jest (`describe` / `it` / `expect`) em um ambiente nativo real: simulador, emulador ou dispositivo físico. Isso o torna compatível com Nitro Modules, incluindo esta biblioteca.

```ts
import { describe, it, expect, beforeEach } from 'react-native-harness'
import { JSDOM } from 'react-native-nitro-jsdom'

describe('script do badge de preço', () => {
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

  it('aplica 10% de desconto', async () => {
    const result = await dom.evaluate(`
      const el = document.getElementById('price')
      el.textContent = String((parseFloat(el.dataset.original) * 0.9).toFixed(2))
      el.textContent
    `)
    expect(result).toBe('90.00')
  })

  it('preserva o valor original no data attribute', async () => {
    const result = await dom.evaluate(
      `document.getElementById('price').dataset.original`
    )
    expect(result).toBe('100')
  })
})
```

## O que essa abordagem cobre e o que não cobre

Essa abordagem é ideal para testar a lógica JavaScript que roda dentro de uma WebView: consultas ao DOM, mutações, valores computados e efeitos colaterais de scripts.

Ela não substitui testes de componente. Se você precisa verificar que o `onMessage` dispara, que o `injectedJavaScriptBeforeContentLoaded` executa no momento certo, ou que a WebView monta e renderiza corretamente, use o `react-native-harness` com um componente `WebView` real.
