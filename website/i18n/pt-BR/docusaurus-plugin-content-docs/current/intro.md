---
id: intro
title: Introdução
sidebar_position: 1
---

# react-native-nitro-jsdom

> Um ambiente headless de HTML/DOM para React Native, desenvolvido com Nitro Modules, Lexbor e QuickJS.

## O Problema

O React Native não possui um equivalente nativo ao [jsdom](https://github.com/jsdom/jsdom). Quando você precisa fazer parsing de um documento HTML, manipular seu DOM ou executar JavaScript arbitrário dentro dele, sem renderizar nada na tela, sua única opção hoje é uma `WebView`, que:

- É um **componente visual**, forçando sua entrada na árvore do React
- Torna **casos de uso headless / apenas lógica impossíveis** de isolar de forma limpa
- Acopla sua lógica de negócio à camada de UI
- Tem **alto overhead**: um motor de navegador completo apenas para avaliar um script

Um exemplo real e concreto: apps que renderizam widgets de conteúdo vindos de um CMS, payloads de HTML com um pequeno script embutido que calcula um valor (um contador regressivo, uma saudação personalizada, um selo de desconto) antes de ser exibido. Na web, esse script simplesmente roda dentro de um `<iframe>`. No React Native, os desenvolvedores são forçados a usar uma `WebView` oculta, uma abstração fundamentalmente quebrada que arrasta toda a camada de UI para o que deveria ser lógica pura.

O **react-native-nitro-jsdom** resolve isso fornecendo um sandbox real e isolado de HTML + JS que roda inteiramente fora da tela e fora da árvore do React.

## O Que Ele Faz

```ts
import { JSDOM } from 'react-native-nitro-jsdom'

const dom = JSDOM.create(`
  <html>
    <body>
      <div id="result">0</div>
    </body>
  </html>
`)

// Manipula o DOM via evaluate(), o único caminho de acesso ao DOM
await dom.evaluate(`
  document.getElementById('result').textContent = String(2 + 2)
`)

// Lê valores de volta via evaluate()
const value = await dom.evaluate(`document.getElementById('result').textContent`)
// → "4"

// Obtém o HTML final
const html = dom.serialize()

// Libera a memória nativa
dom.dispose()
```

`evaluate()` é a única porta de entrada para o sandbox: todas as consultas e mutações do DOM são expressas como strings JavaScript passadas ao QuickJS, que delega para o Lexbor internamente. A API é intencionalmente compatível com o [jsdom](https://github.com/jsdom/jsdom), então código escrito para Node.js deve migrar com mudanças mínimas.

## Comparação

| Recurso | WebView (oculta) | react-native-nitro-jsdom |
|---|---|---|
| Requer componente React | Sim | Não |
| Roda headless / fora da árvore | Não | Sim |
| Instâncias isoladas | Complexo | Nativo |
| Execução de JS arbitrário | Sim | Sim |
| API completa de DOM | Sim | Progressiva |
| Controle de memória | Limitado | Total (`dispose()`) |
| Overhead de performance | Alto | Baixo |
