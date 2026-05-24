import type { HtmlSandbox } from "../../specs/HtmlSandbox.nitro";
import type { IJSDOMOptions } from "./types";
import { NitroModules } from "react-native-nitro-modules";

export class JSDOM {
  private sandbox: HtmlSandbox | null;
  private _disposed: boolean = false;

  private constructor(sandbox: HtmlSandbox) {
    this.sandbox = sandbox;
  }

  /**
   * Creates a new sandboxed DOM environment.
   *
   * @param html - The initial HTML document string.
   * @param options.runScripts - Execute `<script>` tags found in the HTML. Default: `true`.
   * @param options.url - Value of `window.location.href` inside the sandbox. Default: `'about:blank'`.
   * @param options.pretendToBeVisual - Sets `document.hidden = false`, allowing scripts that
   *   gate on page visibility (e.g. `requestAnimationFrame`) to run. Default: `false`.
   *
   * @example
   * ```ts
   * const dom = JSDOM.create('<html><body><div id="result">0</div></body></html>')
   * await dom.evaluate(`document.getElementById('result').textContent = String(2 + 2)`)
   * const value = await dom.evaluate(`document.getElementById('result').textContent`)
   * // value → "4"
   * dom.dispose()
   * ```
   */
  static create(html: string, options?: IJSDOMOptions): JSDOM {
    const sandbox = NitroModules.createHybridObject<HtmlSandbox>('HtmlSandbox');

    sandbox.initialize(
      html,
      options?.runScripts ?? true,
      options?.url ?? 'about:blank',
    );
    return new JSDOM(sandbox);
  }

  /**
   * Runs arbitrary JavaScript inside the isolated QuickJS sandbox.
   * The return value is coerced to a string (same behaviour as `String(expr)` in JS).
   *
   * This is the **only** door into the sandbox — all DOM queries and mutations must
   * be expressed as JS strings passed to `evaluate()`.
   *
   * @param script - Any valid JS expression or block of statements.
   * @returns The stringified result of the last evaluated expression.
   *
   * @example
   * ```ts
   * // Mutate the DOM
   * await dom.evaluate(`document.getElementById('price').textContent = (qty * unitPrice).toFixed(2)`)
   *
   * // Read from the DOM
   * const text = await dom.evaluate(`document.getElementById('price').textContent`)
   * // text → "42.00"
   * ```
   */
  evaluate(script: string): Promise<string> {
    if (this._disposed) {
      return Promise.reject(new Error("JSDOM instance has been disposed"));
    }
    return this.sandbox!.evaluate(script);
  }

  /**
   * Serializes the current state of the document back to an HTML string.
   * Reflects any DOM mutations made via `evaluate()`.
   *
   * Returns an empty string if called after `dispose()`.
   *
   * @example
   * ```ts
   * await dom.evaluate(`document.title = 'Hello'`)
   * dom.serialize() // "<html><head><title>Hello</title></head>...</html>"
   * ```
   */
  serialize(): string {
    if (this._disposed) return "";
    return this.sandbox!.serialize();
  }

  /**
   * Frees all native memory held by this sandbox (Lexbor document + QuickJS runtime).
   * After calling `dispose()`, any further `evaluate()` calls will reject with an error.
   * Calling `dispose()` multiple times is safe (idempotent).
   * Always call this when you are done with the sandbox to avoid memory leaks.
   *
   * @example
   * ```ts
   * const dom = JSDOM.create(html)
   * const value = await dom.evaluate(`window.__result`)
   * dom.dispose() // ← always pair with create()
   * ```
   */
  dispose(): void {
    if (this._disposed) return;
    this._disposed = true;
    this.sandbox = null;
  }
}
