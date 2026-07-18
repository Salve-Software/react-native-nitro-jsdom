---
id: roadmap
title: Roadmap
sidebar_position: 5
---

# Roadmap

### v0.1: MVP
- [x] Spec Nitro + API TypeScript (`JSDOM.create`, `evaluate`, `serialize`, `dispose`)
- [x] Estrutura C++ (`HybridHtmlSandbox`, `LexborDocument`, `QuickJSRuntime`, `DOMBindings`)
- [x] Integração com Lexbor: parsing real de HTML e consultas de DOM
- [x] Integração com QuickJS: execução real de JS
- [x] Suporte a iOS
- [x] Suporte a Android

### v0.2: DOM real dentro do evaluate()
- [x] Conectar o parsing real de HTML do Lexbor para que `evaluate()` veja um DOM vivo
- [x] Conectar a execução de JS do QuickJS para que scripts rodem dentro do sandbox
- [x] `document.querySelector(sel)` / `document.querySelectorAll(sel)` retornam objetos de elemento reais dentro de `evaluate()`
- [x] Getter e setter de `element.textContent` / `element.innerHTML` acessíveis dentro de `evaluate()`
- [x] `document.getAttribute(sel, attr)` / `document.setAttribute(sel, attr, val)` dentro de `evaluate()`
- [x] `document.createElement` / `element.appendChild` / `element.removeChild` dentro de `evaluate()`

### v0.3: Assincronismo & Eventos
- [x] `addEventListener` / `removeEventListener`
- [x] `setTimeout` / `setInterval` / `clearTimeout` / `clearInterval`
- [x] Suporte a `Promise` dentro do sandbox (drenagem de microtasks + propagação de rejeições não tratadas)
- [x] Encaminhamento de `console.log` para o console do RN via opção `onConsole`
- [x] `dispatchEvent(new Event('type'))` dispara para os listeners registrados
- [x] `evaluate()` drena completamente o event loop antes de retornar

### v0.4: Rede & Armazenamento
- [x] `fetch` (conectado à stack de rede do RN)
- [x] Stubs de `localStorage` / `sessionStorage`
- [x] Stub de `XMLHttpRequest`

### v0.5: Compatibilidade com jsdom
- [x] Auditoria completa de paridade com a API do jsdom
- [x] `window.location`
- [x] `MutationObserver`
- [x] `window.alert` / `window.confirm` / `window.prompt`
- [x] `CustomEvent`

### v0.6: API de Node & Style
> Lacunas identificadas pela auditoria de paridade v0.5 com o jsdom, priorizadas
> pela frequência com que scripts HTML/JS embutidos do mundo real as utilizam.
- [x] Travessia genérica de Node (`childNodes`, `nodeType`, `nodeName`, `nodeValue`, `firstChild` / `lastChild` / `nextSibling` / `previousSibling`, `parentNode`)
- [x] `document.getElementsByClassName` / `document.getElementsByTagName`
- [x] `element.cloneNode()`
- [x] `element.dataset` (espelha os atributos `data-*`)
- [x] `element.style` (objeto no estilo `CSSStyleDeclaration`)
- [x] `document.title`
- [x] Bubbling de eventos real (`dispatchEvent` percorre os ancestrais; `stopPropagation()` / `preventDefault()` têm efeito)

### v0.7: Identidade de Node & Ergonomia de DOM
> Lacunas identificadas comparando com o jsdom para scripts embutidos do mundo real:
> identidade estável de node mais os métodos de travessia/mutação que esses scripts
> mais utilizam. APIs dependentes de layout (`getComputedStyle`,
> `getBoundingClientRect`, CSSOM completo) estão fora de escopo, já que não há
> renderização para sustentá-las.
- [x] Identidade estável de node (`el.firstChild === el.firstChild`) via um cache de wrappers por runtime
- [x] `node.contains(other)`
- [x] `element.closest(selector)`
- [x] `node.replaceChild(newChild, oldChild)`
- [x] `node.before(...nodes)` / `after(...nodes)` / `replaceWith(...nodes)`
- [x] `element.append(...nodes)` / `prepend(...nodes)`
- [x] `element.insertAdjacentHTML(position, html)`

### v0.8: Enumeração de Atributos & Comparação de Nodes
- [x] `document.createComment()` / `document.createDocumentFragment()`
- [x] `element.getAttributeNames()`
- [x] `element.attributes` (array snapshot de `{name, value}`)
- [x] `element.toggleAttribute(name, force?)`
- [x] `node.isSameNode(other)` / `node.isEqualNode(other)`
