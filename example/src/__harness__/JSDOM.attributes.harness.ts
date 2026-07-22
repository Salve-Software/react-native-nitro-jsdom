import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM attributes/dataset/style', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('dataset reads and writes data-* attributes with camelCase mapping', async () => {
    dom = JSDOM.create('<html><body><div id="d" data-user-id="42" data-role="admin"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const before = { userId: div.dataset.userId, role: div.dataset.role };
      div.dataset.userId = '99';
      div.dataset.newFlag = 'yes';
      delete div.dataset.role;
      JSON.stringify({
        before,
        after: {
          userId: div.dataset.userId,
          newFlag: div.dataset.newFlag,
          role: div.dataset.role,
        },
        attrUserId: div.getAttribute('data-user-id'),
        attrNewFlag: div.getAttribute('data-new-flag'),
        attrRole: div.getAttribute('data-role'),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      before: { userId: '42', role: 'admin' },
      after: { userId: '99', newFlag: 'yes', role: undefined },
      attrUserId: '99',
      attrNewFlag: 'yes',
      attrRole: null,
    });
  });

  it('style reads/writes inline CSS via camelCase properties and cssText', async () => {
    dom = JSDOM.create('<html><body><div id="d" style="color: red; font-size: 12px;"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const before = { color: div.style.color, fontSize: div.style.fontSize };
      div.style.backgroundColor = 'blue';
      div.style.setProperty('margin-top', '4px');
      const afterGetPropertyValue = div.style.getPropertyValue('margin-top');
      div.style.removeProperty('color');
      JSON.stringify({
        before,
        backgroundColor: div.style.backgroundColor,
        afterGetPropertyValue,
        colorAfterRemove: div.style.color === undefined,
        cssText: div.style.cssText,
        attrStyle: div.getAttribute('style'),
      });
    `);
    const parsed = JSON.parse(result);
    expect(parsed.before).toEqual({ color: 'red', fontSize: '12px' });
    expect(parsed.backgroundColor).toBe('blue');
    expect(parsed.afterGetPropertyValue).toBe('4px');
    expect(parsed.colorAfterRemove).toBe(true);
    expect(parsed.cssText).toBe(parsed.attrStyle);
    expect(parsed.cssText).not.toContain('color: red');
    expect(parsed.cssText).toContain('background-color: blue');
    expect(parsed.cssText).toContain('margin-top: 4px');
  });

  it('getAttributeNames()/attributes list every attribute on the element', async () => {
    dom = JSDOM.create('<html><body><div id="d" class="x" data-role="admin"></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      JSON.stringify({
        names: div.getAttributeNames(),
        attrs: Array.from(div.attributes).map((a) => [a.name, a.value]),
      });
    `);
    expect(JSON.parse(result)).toEqual({
      names: ['id', 'class', 'data-role'],
      attrs: [
        ['id', 'd'],
        ['class', 'x'],
        ['data-role', 'admin'],
      ],
    });
  });

  it('toggleAttribute() toggles by default and respects the force argument', async () => {
    dom = JSDOM.create('<html><body><div id="d" hidden></div></body></html>');
    const result = await dom.evaluate(`
      const div = document.getElementById('d');
      const removed = div.toggleAttribute('hidden');
      const afterRemove = div.hasAttribute('hidden');
      const added = div.toggleAttribute('hidden');
      const afterAdd = div.hasAttribute('hidden');
      const forcedFalseOnAbsent = div.toggleAttribute('missing', false);
      const forcedTrueTwice = [div.toggleAttribute('data-x', true), div.toggleAttribute('data-x', true)];
      JSON.stringify({ removed, afterRemove, added, afterAdd, forcedFalseOnAbsent, forcedTrueTwice });
    `);
    expect(JSON.parse(result)).toEqual({
      removed: false,
      afterRemove: false,
      added: true,
      afterAdd: true,
      forcedFalseOnAbsent: false,
      forcedTrueTwice: [true, true],
    });
  });

  it('getBoundingClientRect() returns a zeroed rect instead of throwing', async () => {
    dom = JSDOM.create('<html><body><div id="d"></div></body></html>');
    const result = await dom.evaluate(`
      JSON.stringify(document.getElementById('d').getBoundingClientRect());
    `);
    expect(JSON.parse(result)).toEqual({
      x: 0, y: 0, width: 0, height: 0, top: 0, right: 0, bottom: 0, left: 0,
    });
  });
});
