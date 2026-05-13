import type { HtmlSandbox } from "../../specs/HtmlSandbox.nitro";
import type { IJSDOMOptions } from "./types";
import { NitroModules } from "react-native-nitro-modules";

export class JSDOM {
  private sandbox: HtmlSandbox;

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
   * const value = dom.getTextContent('#result') // "4"
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
   * @param script - Any valid JS expression or block of statements.
   * @returns The stringified result of the last evaluated expression.
   *
   * @example
   * ```ts
   * const result = await dom.evaluate(`
   *   document.getElementById('price').textContent = (qty * unitPrice).toFixed(2)
   *   document.getElementById('price').textContent
   * `)
   * // result → "42.00"
   * ```
   */
  evaluate(script: string): Promise<string> {
    return this.sandbox.evaluate(script);
  }

  /**
   * Serializes the current state of the document back to an HTML string.
   * Reflects any DOM mutations made via `evaluate`, `setAttribute`, or `setInnerHTML`.
   *
   * @example
   * ```ts
   * await dom.evaluate(`document.title = 'Hello'`)
   * dom.serialize() // "<html><head><title>Hello</title></head>...</html>"
   * ```
   */
  serialize(): string {
    return this.sandbox.serialize();
  }

  /**
   * Returns the serialized outer HTML of the first element matching the CSS selector, or `null` if not found.
   *
   * @param selector - Any valid CSS selector (e.g. `'#id'`, `'.class'`, `'div > p'`).
   *
   * @example
   * ```ts
   * dom.querySelector('#result') // "<div id=\"result\">4</div>"
   * dom.querySelector('.missing') // null
   * ```
   */
  querySelector(selector: string): string | null {
    return this.sandbox.querySelector(selector);
  }

  /**
   * Returns the serialized outer HTML of every element matching the CSS selector.
   * Returns an empty array if nothing matches.
   *
   * @param selector - Any valid CSS selector.
   *
   * @example
   * ```ts
   * dom.querySelectorAll('.item') // ["<li class=\"item\">A</li>", "<li class=\"item\">B</li>"]
   * ```
   */
  querySelectorAll(selector: string): string[] {
    return this.sandbox.querySelectorAll(selector);
  }

  /**
   * Returns the value of an attribute on the first element matching the selector, or `null` if
   * the element or attribute does not exist.
   *
   * @param selector - CSS selector targeting the element.
   * @param attr - Attribute name (e.g. `'data-value'`, `'href'`, `'class'`).
   *
   * @example
   * ```ts
   * dom.getAttribute('#btn', 'data-action') // "submit"
   * dom.getAttribute('#btn', 'nonexistent') // null
   * ```
   */
  getAttribute(selector: string, attr: string): string | null {
    return this.sandbox.getAttribute(selector, attr);
  }

  /**
   * Sets an attribute on the first element matching the selector.
   * Creates the attribute if it does not exist; silently does nothing if the element is not found.
   *
   * @param selector - CSS selector targeting the element.
   * @param attr - Attribute name.
   * @param value - New attribute value.
   *
   * @example
   * ```ts
   * dom.setAttribute('#result', 'data-computed', 'true')
   * ```
   */
  setAttribute(selector: string, attr: string, value: string): void {
    this.sandbox.setAttribute(selector, attr, value);
  }

  /**
   * Returns the `textContent` of the first element matching the selector — i.e. all text
   * inside the element with HTML tags stripped — or `null` if not found.
   *
   * @param selector - CSS selector targeting the element.
   *
   * @example
   * ```ts
   * // <div id="result"><span>42</span></div>
   * dom.getTextContent('#result') // "42"
   * ```
   */
  getTextContent(selector: string): string | null {
    return this.sandbox.getTextContent(selector);
  }

  /**
   * Returns the `innerHTML` of the first element matching the selector — the serialized HTML
   * of its children — or `null` if not found.
   *
   * @param selector - CSS selector targeting the element.
   *
   * @example
   * ```ts
   * // <div id="root"><span>hello</span></div>
   * dom.getInnerHTML('#root') // "<span>hello</span>"
   * ```
   */
  getInnerHTML(selector: string): string | null {
    return this.sandbox.getInnerHTML(selector);
  }

  /**
   * Replaces the children of the first element matching the selector by parsing the given HTML fragment.
   * Silently does nothing if the element is not found.
   *
   * @param selector - CSS selector targeting the element.
   * @param html - HTML fragment to set as the element's new inner content.
   *
   * @example
   * ```ts
   * dom.setInnerHTML('#root', '<strong>updated</strong>')
   * dom.getInnerHTML('#root') // "<strong>updated</strong>"
   * ```
   */
  setInnerHTML(selector: string, html: string): void {
    this.sandbox.setInnerHTML(selector, html);
  }

  /**
   * Frees all native memory held by this sandbox (Lexbor document + QuickJS runtime).
   * After calling `dispose()`, any further method calls on this instance will throw.
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
    this.sandbox.dispose();
  }
}
