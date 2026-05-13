import type { HybridObject } from 'react-native-nitro-modules';

export interface HtmlSandbox extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  // Must be called once right after createHybridObject()
  initialize(html: string, runScripts: boolean, url: string): void;

  // Run arbitrary JS inside the QuickJS sandbox; returns the result as a string
  evaluate(script: string): Promise<string>;

  // Returns the current HTML of the document
  serialize(): string;

  // DOM query methods
  querySelector(selector: string): string | null;
  querySelectorAll(selector: string): string[];

  // DOM attribute manipulation
  getAttribute(selector: string, attr: string): string | null;
  setAttribute(selector: string, attr: string, value: string): void;

  // Text / inner HTML
  getTextContent(selector: string): string | null;
  getInnerHTML(selector: string): string | null;
  setInnerHTML(selector: string, html: string): void;
}
