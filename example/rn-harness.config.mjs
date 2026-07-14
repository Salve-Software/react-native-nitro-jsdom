import {
  androidPlatform,
  androidEmulator,
} from '@react-native-harness/platform-android';
import {
  applePlatform,
  appleSimulator,
} from '@react-native-harness/platform-apple';

// HtmlSandbox only ships ios/android native implementations (see
// `HybridObject<{ ios: 'c++'; android: 'c++' }>` in src/specs/HtmlSandbox.nitro.ts),
// so there's no web runner here.
//
// Adjust device/emulator names below to match what's available on your machine
// (`xcrun simctl list devices` / `emulator -list-avds`).
const config = {
  entryPoint: './index.js',
  appRegistryComponentName: 'NitroJsdomExample',

  runners: [
    androidPlatform({
      name: 'android',
      device: androidEmulator('Pixel_8_API_35', {
        apiLevel: 35,
        profile: 'pixel_8',
      }),
      bundleId: 'com.nitrojsdomexample',
    }),
    applePlatform({
      name: 'ios',
      device: appleSimulator('iPhone 16 Pro', '18.0'),
      bundleId: 'com.nitrojsdomexample',
    }),
  ],
};

export default config;
