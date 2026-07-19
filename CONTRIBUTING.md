# Contributing

We welcome contributions! Here's how you can help.

## Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/your-username/react-native-nitro-jsdom.git
   ```
3. Install the submodules (Lexbor and QuickJS are compiled from source):
   ```bash
   git submodule update --init --recursive
   ```
4. Install dependencies:
   ```bash
   yarn install
   yarn codegen
   ```
5. Create a new branch:
   ```bash
   git checkout -b feat/your-feature-name
   ```

## Branch Naming Convention

- `feat/` - New features
- `fix/` - Bug fixes
- `docs/` - Documentation updates
- `refactor/` - Code refactoring
- `test/` - Adding or updating tests

## Commit Message Format

Follow conventional commits:

```
type(scope): description

Examples:
feat(sandbox): add MutationObserver support
fix(bindings): correct textContent setter on detached nodes
docs(readme): update installation instructions
```

## Pull Request Process

1. Ensure your code follows the existing style
2. Update documentation if needed
3. Run `yarn typecheck` and test with the example app (`yarn example ios` / `yarn example android`)
4. Create a Pull Request with a clear description

## Running the Example App

```bash
yarn --cwd example pod   # iOS only
yarn example ios         # or: yarn example android
```

## Running the Documentation Site

```bash
yarn docs       # build + serve, matches production (locale switching included)
yarn docs:dev   # fast dev server with hot reload, single locale
```
