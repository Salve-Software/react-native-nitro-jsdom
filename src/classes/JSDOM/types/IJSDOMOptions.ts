import type { IFetchRequestInit, IFetchResponseInit } from './IFetchOptions';

export interface IJSDOMOptions {
  /** Execute <script> tags found in the initial HTML. Default: true */
  runScripts?: boolean;
  /** Value of window.location.href inside the sandbox. Default: 'about:blank' */
  url?: string;
  /** Sets document.hidden = false. Default: false */
  pretendToBeVisual?: boolean;
  /**
   * Callback invoked whenever console.log / warn / error / info / debug is called
   * inside the sandbox. If not provided, console output is silently discarded.
   */
  onConsole?: (level: 'log' | 'warn' | 'error' | 'info' | 'debug', args: string[]) => void;
  /**
   * Callback invoked when `window.alert(message)` is called inside the sandbox.
   * If not provided, alert() is a silent no-op (browser default when no UI is available).
   *
   * WARNING: The callback is synchronous. Do NOT call back into the same sandbox
   * from inside the callback — QuickJS is not re-entrant.
   */
  onAlert?: (message: string) => void;
  /**
   * Callback invoked when `window.confirm(message)` is called inside the sandbox.
   * Return a boolean, or a `Promise<boolean>`. If not provided, confirm() returns
   * false (browser default).
   *
   * WARNING: Do NOT call back into the same sandbox from inside the callback —
   * QuickJS is not re-entrant.
   */
  onConfirm?: (message: string) => boolean | Promise<boolean>;
  /**
   * Callback invoked when `window.prompt(message, defaultValue?)` is called inside
   * the sandbox. Return a string for the user input, or null for a dismissed prompt.
   * If not provided, prompt() returns null (browser default for dismissed prompts).
   *
   * WARNING: The callback is synchronous. Do NOT call back into the same sandbox
   * from inside the callback — QuickJS is not re-entrant.
   */
  onPrompt?: (message: string, defaultValue?: string) => string | null;
  /**
   * Callback invoked when `fetch(url, init)` is called inside the sandbox.
   * Receives the request and must resolve with the response. If not provided,
   * `fetch()` inside the sandbox rejects with "fetch is not available".
   *
   * Typically wired to React Native's real `fetch()`:
   * ```ts
   * onFetch: async (url, init) => {
   *   const res = await fetch(url, init)
   *   return { status: res.status, statusText: res.statusText, headers: Object.fromEntries(res.headers), body: await res.text() }
   * }
   * ```
   *
   * WARNING: Do NOT call back into the same sandbox (`dom.evaluate()`) from inside
   * this callback — QuickJS is not re-entrant.
   */
  onFetch?: (url: string, init: IFetchRequestInit) => Promise<IFetchResponseInit>;
}