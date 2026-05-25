import type { HybridObject } from 'react-native-nitro-modules';

export interface HtmlSandbox extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  // Must be called once right after createHybridObject()
  initialize(html: string, runScripts: boolean, url: string): void;

  // Run arbitrary JS inside the QuickJS sandbox; returns the result as a string
  evaluate(script: string): Promise<string>;

  // Returns the current HTML of the document
  serialize(): string;

  // Set (or clear) the console output callback. Pass null to silence console output.
  setConsoleCallback(callback: ((level: string, args: string[]) => void) | null): void;
}
