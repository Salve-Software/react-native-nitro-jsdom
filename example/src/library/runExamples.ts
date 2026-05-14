import type { IResult } from '../types';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

export const runExamples = async (): Promise<IResult[]> => {
  const html = `
    <html>
      <body>
        <div id="result">0</div>
        <p class="item">Hello</p>
        <p class="item">World</p>
      </body>
    </html>
  `

  const dom = JSDOM.create(html);

  const evalResult = await dom.evaluate(
    `document.setInnerHTML('#result', String(2 + 2)); document.getTextContent('#result')`
  );

  const textContent = dom.getTextContent('#result') ?? '(null)';
  const serialized = dom.serialize();

  const items = dom.querySelectorAll('.item');

  dom.dispose();

  return [
    { label: 'evaluate() → 2+2', value: evalResult || '(empty)' },
    { label: 'getTextContent(#result)', value: textContent },
    { label: 'querySelectorAll(.item).length', value: String(items.length) },
    { label: 'serialize() length', value: `${serialized.length} chars` },
  ];
}
