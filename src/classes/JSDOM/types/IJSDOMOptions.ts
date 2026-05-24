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
}