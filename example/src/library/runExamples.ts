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

  // Mutate the DOM via evaluate() — the only DOM access path
  await dom.evaluate(`document.setInnerHTML('#result', String(2 + 2))`);

  // Read back via evaluate()
  const evalResult = await dom.evaluate(`document.getTextContent('#result')`);

  // Count items via evaluate()
  const itemCount = await dom.evaluate(`document.querySelectorAll('.item').length`);

  const serialized = dom.serialize();

  dom.dispose();

  return [
    { label: 'evaluate() → 2+2', value: evalResult || '(empty)' },
    { label: 'evaluate() → querySelectorAll(.item).length', value: itemCount || '(empty)' },
    { label: 'serialize() length', value: `${serialized.length} chars` },
  ];
}
