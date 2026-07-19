---
id: compatibility
title: Compatibility
sidebar_position: 4
---

# Compatibility

## Requirements

| | Minimum |
|---|---|
| React Native | 0.76.0 |
| Node.js | 18.0.0 |
| `react-native-nitro-modules` | Required peer dependency, matching version |

```bash
yarn add react-native-nitro-jsdom react-native-nitro-modules
```

`react-native-nitro-modules` is not bundled: every consumer must install it directly, since it's the native bridge Nitro-based libraries share.

## New Architecture required

This library is built entirely on [Nitro Modules](https://nitro.margelo.com/), which compile down to JSI/TurboModules. There is no legacy bridge fallback, so the **New Architecture (Fabric + TurboModules) must be enabled** in your app. React Native 0.76 is the version where the New Architecture became stable enough to rely on as a floor.

## Platforms

- **iOS**: supported, deployment target inherited from your React Native / CocoaPods setup (no separate override)
- **Android**: supported
- **visionOS**: supported, 1.0 or higher

### Android build details

| | Value |
|---|---|
| `minSdkVersion` | 23 |
| `compileSdkVersion` | 34 |
| NDK | 27.1.12297006 |
| C++ standard | C++20 |

## JS engine

Tested with **Hermes**. JSC has not been specifically verified. QuickJS (the engine used *inside* the sandbox) runs independently of whichever engine powers the rest of your app, so this only affects your app's own JS, not code passed to `evaluate()`.

## Expo

The example app in this repo is a plain React Native CLI project, not Expo. Because this library ships native code compiled via Nitro Modules, it cannot run inside **Expo Go**. It should work with a custom **Expo Dev Client** (`expo prebuild` + a development build), but that path hasn't been verified yet.
