import type { HybridObject } from 'react-native-nitro-modules';

export interface HtmlSandbox extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  // Must be called once right after createHybridObject()
  initialize(html: string, runScripts: boolean, url: string, pretendToBeVisual: boolean): void;

  // Run arbitrary JS inside the QuickJS sandbox; returns the result as a string
  evaluate(script: string): Promise<string>;

  // Returns the current HTML of the document
  serialize(): string;

  // Set (or clear) the console output callback. Pass null to silence console output.
  setConsoleCallback(callback: ((level: string, args: string[]) => void) | null): void;

  // Set (or clear) the dialog callbacks for window.alert / confirm / prompt.
  // Pass null for any callback to use the browser default (no-op / false / null).
  setDialogCallbacks(
    onAlert: ((message: string) => void) | null,
    onConfirm: ((message: string) => Promise<boolean>) | null,
    onPrompt: ((message: string, defaultValue?: string) => string | null) | null,
  ): void;

  // Set (or clear) the fetch() bridge. Pass null to disable fetch() inside the sandbox
  // (calling it will reject). The callback receives (url, method, headersJson, body) and
  // must resolve with a JSON-encoded response: {ok, status, statusText, headersJson, body}.
  // Declared as returning Promise<string> (not `string`) so Nitro's JSIConverter for
  // Promise<T> properly chains `.then()` on the real async JS callback — a plain `string`
  // return type is only auto-dispatched across threads, it does NOT await a returned Promise.
  setFetchCallback(
    callback: ((url: string, method: string, headersJson: string, body?: string) => Promise<string>) | null,
  ): void;
}
