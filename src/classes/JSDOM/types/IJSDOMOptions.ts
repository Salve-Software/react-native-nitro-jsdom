export interface IJSDOMOptions {
  /** Execute <script> tags found in the initial HTML. Default: true */
  runScripts?: boolean;
  /** Value of window.location.href inside the sandbox. Default: 'about:blank' */
  url?: string;
  /** Sets document.hidden = false. Default: false */
  pretendToBeVisual?: boolean;
}