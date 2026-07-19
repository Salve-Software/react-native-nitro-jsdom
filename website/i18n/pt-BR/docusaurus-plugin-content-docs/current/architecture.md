---
id: architecture
title: Arquitetura
sidebar_position: 2
---

# Arquitetura

```
TypeScript (consumidor)
        │
        │  chamada JSI síncrona, sem bridge, sem serialização JSON
        ▼
Nitro Module (camada de binding em C++)
        │
        ├──► Lexbor (C99)
        │      ├── Parsing de HTML compatível com WHATWG
        │      ├── Árvore DOM em memória
        │      ├── querySelector / querySelectorAll
        │      ├── getAttribute / setAttribute
        │      ├── textContent / innerHTML
        │      └── serialize() → string HTML
        │
        └──► QuickJS (C)
               ├── Runtime JS isolado por instância
               ├── stubs de window / document
               ├── Bindings de DOM → delega para o Lexbor
               ├── setTimeout / setInterval (event loop próprio)
               ├── console.log → encaminhado para o RN
               └── Executa scripts arbitrários do usuário
```

## Por que essas tecnologias?

| Camada | Escolha | Motivo |
|---|---|---|
| Binding nativo | **Nitro Modules** | Bindings JSI síncronos e type-safe. Zero overhead de bridge |
| HTML + DOM | **Lexbor** | O parser de HTML compatível com WHATWG mais rápido em C99, sem dependências |
| Motor JS | **QuickJS** | Construído para embedding. Leve, totalmente isolado, suporte a ES2023 |
| Cola | **C++** | Conecta QuickJS ↔ Lexbor ↔ Nitro/JSI |

:::info[Por que não reutilizar a instância do Hermes que o React Native já usa?]
O runtime Hermes é uma instância única compartilhada, não projetada para múltiplos ambientes isolados. O QuickJS foi construído especificamente para embedding e isolamento, então cada chamada a `JSDOM.create()` recebe seu próprio runtime independente.
:::
