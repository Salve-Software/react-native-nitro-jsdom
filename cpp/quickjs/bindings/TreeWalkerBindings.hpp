#pragma once

// NodeFilter, TreeWalker, NodeIterator, and document.createTreeWalker() /
// createNodeIterator(). Pure JS on top of the Node traversal properties
// ElementBindings already exposes (firstChild/lastChild/nextSibling/
// previousSibling/parentNode/nodeType) — no Lexbor access needed.
//
// The traversal algorithms (TreeWalker.previousSibling()/nextSibling() in
// particular — they can descend into an accepted-skip subtree looking for a
// matching descendant, then ascend) are subtle enough that this is a direct
// port of jsdom's TreeWalker-impl.js/NodeIterator-impl.js/helpers.js, not a
// from-scratch implementation, to avoid a plausible-looking-but-wrong
// backtracking bug. NodeIterator's `_referenceNode`/`_pointerBeforeReferenceNode`
// walk uses simple tree-order successor/predecessor helpers here in place of
// jsdom's domSymbolTree.following()/preceding() dependency.
//
// Must run after ElementBindings (globalThis.Element/Node) and
// DocumentBindings (globalThis.document).

#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

struct TreeWalkerBindings {
  static void install(JSContext* ctx);
};

} // namespace margelo::nitro::nitrojsdom
