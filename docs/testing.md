# Testing C++ bindings

> Applies to every change under `cpp/` (Nitro glue, `LexborDocument`, `QuickJSRuntime`,
> and everything in `cpp/quickjs/bindings/`).

There is no way to unit-test QuickJS/Lexbor C++ in isolation — the only thing that
proves a binding works is running it through the real Nitro module on a real
device/simulator. That happens via `react-native-harness`, in
`example/src/__harness__/`.

**Whenever you implement or change a C++ binding, you must add or extend a
harness test file covering it.** A binding without a harness test is not done.

---

## Where tests live

One file per feature area: `example/src/__harness__/JSDOM.<feature>.harness.ts`
(e.g. `JSDOM.shadowdom.harness.ts`, `JSDOM.customelements.harness.ts`,
`JSDOM.mutation.harness.ts`). If you're extending an existing binding, add cases
to its existing file instead of creating a new one.

Files are picked up automatically by `example/jest.harness.config.mjs`
(`testMatch: ['**/*.harness.ts', ...]`) — no registration needed anywhere else.

## File shape

Every harness file follows this exact skeleton:

```ts
import { describe, it, expect, afterEach } from 'react-native-harness';
import { JSDOM } from '@salve-software/react-native-nitro-jsdom';

// Runs on a real device/simulator via react-native-harness, exercising the actual
// Nitro/QuickJS/Lexbor native module — not a JS mock.

describe('JSDOM <feature name>', () => {
  let dom: JSDOM | undefined;

  afterEach(() => {
    dom?.dispose();
    dom = undefined;
  });

  it('describes one specific, observable behavior', async () => {
    dom = JSDOM.create('<html><body>...</body></html>');
    const result = await dom.evaluate(`
      ...
      JSON.stringify({ ... });
    `);
    expect(JSON.parse(result)).toEqual({ ... });
  });
});
```

Rules that keep every file consistent:

- `dom` is always disposed in `afterEach`, never manually at the end of each `it`.
- Every `evaluate()` call is a single template-literal script. When the result is
  a plain string, compare with `.toBe(...)` directly; for anything structured,
  end the script with `JSON.stringify({...})` and assert with
  `expect(JSON.parse(result)).toEqual({...})`.
- Script bodies that need `await` wrap themselves in an async IIFE and `return`
  the value, since `evaluate()` only sees the result of the final expression:
  ```js
  (async () => {
    ...
    return JSON.stringify(order);
  })()
  ```
  (`evaluate()` drains the event loop before resolving, so this works without
  extra plumbing.)
- Thrown errors are asserted by catching inside the script and serializing
  `{ name: e.name, isDOMException: e instanceof DOMException }` (or
  `e.constructor.name` for plain `TypeError`s), not by `.rejects.toThrow()`
  around `evaluate()` — the exception is caught inside the sandbox, not by the
  host promise.
- One `it()` per specific behavior, named as a sentence describing the
  behavior, not the method under test (e.g. `'appendChild()/removeChild() fire
  connectedCallback/disconnectedCallback'`, not `'test appendChild'`).

## What to cover

Read the binding's own header comment first (e.g.
`cpp/quickjs/bindings/CustomElementsBindings.hpp`) — these headers document the
exact contract (triggers, limitations, ordering requirements), and that
contract is the test plan. At minimum, cover:

- The golden path for every new method/property/global exposed.
- Every documented limitation or scope boundary (a header that says "X is not
  hooked" deserves a test showing X, if easy to assert, or at least should not
  be silently contradicted by a passing test).
- Error paths (invalid input, wrong state) and the exact exception type/name
  thrown, since these bindings hand-roll `DOMException`/`TypeError` semantics.
- Interactions with neighboring bindings called out in the "must run after ..."
  comment in the binding's header (e.g. Shadow DOM + Custom Elements).

## Running the tests

```bash
yarn --cwd example test:harness:ios      # or test:harness:android
```

These require a running simulator/device and are not part of `yarn typecheck`
or `yarn build` — always sanity-check new/changed harness files with
`npx tsc --noEmit` from `example/` before considering the work done, even if
you can't run the full harness in your environment.
