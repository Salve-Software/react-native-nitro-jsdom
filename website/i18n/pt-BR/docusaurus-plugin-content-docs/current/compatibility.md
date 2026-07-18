---
id: compatibility
title: Compatibilidade
sidebar_position: 4
---

# Compatibilidade

## Requisitos

| | Mínimo |
|---|---|
| React Native | 0.76.0 |
| Node.js | 18.0.0 |
| `react-native-nitro-modules` | Peer dependency obrigatória, versão correspondente |

```bash
yarn add react-native-nitro-jsdom react-native-nitro-modules
```

O `react-native-nitro-modules` não vem embutido: cada consumidor precisa instalá-lo diretamente, já que é a ponte nativa compartilhada pelas bibliotecas baseadas em Nitro.

## Exige Nova Arquitetura

Esta biblioteca é construída inteiramente sobre [Nitro Modules](https://nitro.margelo.com/), que compilam direto para JSI/TurboModules. Não existe fallback para a bridge legada, então a **Nova Arquitetura (Fabric + TurboModules) precisa estar habilitada** no seu app. O React Native 0.76 é a versão em que a Nova Arquitetura ficou estável o suficiente para ser usada como piso.

## Plataformas

- **iOS**: suportado, deployment target herdado da sua configuração de React Native / CocoaPods (sem override próprio)
- **Android**: suportado
- **visionOS**: suportado, 1.0 ou superior

### Detalhes do build Android

| | Valor |
|---|---|
| `minSdkVersion` | 23 |
| `compileSdkVersion` | 34 |
| NDK | 27.1.12297006 |
| Padrão C++ | C++20 |

## Motor JS

Testado com **Hermes**. O JSC não foi verificado especificamente. O QuickJS (o motor usado *dentro* do sandbox) roda de forma independente do motor que executa o resto do seu app, então isso só afeta o JS do seu próprio app, não o código passado para `evaluate()`.

## Expo

O app de exemplo deste repositório é um projeto React Native CLI puro, não Expo. Como esta biblioteca inclui código nativo compilado via Nitro Modules, ela não roda dentro do **Expo Go**. Deve funcionar com um **Expo Dev Client** customizado (`expo prebuild` + um development build), mas esse caminho ainda não foi verificado.
