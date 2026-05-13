#!/bin/bash
# Generates .clangd-headers/NitroModules/ with relative symlinks so clangd
# can resolve <NitroModules/...> includes without a pod install.
# Run once after cloning: bash scripts/setup-clangd.sh

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="$ROOT/.clangd-headers/NitroModules"

rm -rf "$TARGET"
mkdir -p "$TARGET"

cd "$TARGET"

find ../../node_modules/react-native-nitro-modules/cpp -name "*.hpp" | while read f; do
  ln -sf "$f" "$(basename "$f")"
done

echo "✓ .clangd-headers/NitroModules/ criado com $(ls | wc -l | tr -d ' ') headers"
