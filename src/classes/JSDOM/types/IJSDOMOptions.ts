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
   * Must return a boolean. If not provided, confirm() returns false (browser default).
   *
   * WARNING: The callback is synchronous. Do NOT call back into the same sandbox
   * from inside the callback — QuickJS is not re-entrant.
   */
  onConfirm?: (message: string) => boolean;
  /**
   * Callback invoked when `window.prompt(message, defaultValue?)` is called inside
   * the sandbox. Return a string for the user input, or null for a dismissed prompt.
   * If not provided, prompt() returns null (browser default for dismissed prompts).
   *
   * WARNING: The callback is synchronous. Do NOT call back into the same sandbox
   * from inside the callback — QuickJS is not re-entrant.
   */
  onPrompt?: (message: string, defaultValue?: string) => string | null;
}