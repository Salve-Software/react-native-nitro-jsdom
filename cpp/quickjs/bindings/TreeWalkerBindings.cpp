#include "TreeWalkerBindings.hpp"
#include <cstring>

namespace margelo::nitro::nitrojsdom {

namespace {

const char* kTreeWalkerBootstrapScript = R"JS(
(function() {
  var FILTER_ACCEPT = 1, FILTER_REJECT = 2, FILTER_SKIP = 3;

  var NodeFilter = {
    FILTER_ACCEPT: FILTER_ACCEPT,
    FILTER_REJECT: FILTER_REJECT,
    FILTER_SKIP: FILTER_SKIP,
    SHOW_ALL: 0xFFFFFFFF,
    SHOW_ELEMENT: 0x1,
    SHOW_ATTRIBUTE: 0x2,
    SHOW_TEXT: 0x4,
    SHOW_CDATA_SECTION: 0x8,
    SHOW_ENTITY_REFERENCE: 0x10,
    SHOW_ENTITY: 0x20,
    SHOW_PROCESSING_INSTRUCTION: 0x40,
    SHOW_COMMENT: 0x80,
    SHOW_DOCUMENT: 0x100,
    SHOW_DOCUMENT_TYPE: 0x200,
    SHOW_DOCUMENT_FRAGMENT: 0x400,
    SHOW_NOTATION: 0x800,
  };
  globalThis.NodeFilter = NodeFilter;

  function normalizeFilter(filter) {
    if (filter === undefined || filter === null) return null;
    if (typeof filter === 'function') return filter;
    if (typeof filter.acceptNode === 'function') {
      return function(node) { return filter.acceptNode(node); };
    }
    return null;
  }

  // Shared by TreeWalker and NodeIterator — both store whatToShow/_filterFn/
  // _active the same way, so this reads `this` structurally rather than
  // needing a common base class.
  function applyFilter(walker, node) {
    if (walker._active) {
      throw new DOMException('Recursive node filtering', 'InvalidStateError');
    }
    var n = node.nodeType - 1;
    if (!((1 << n) & walker.whatToShow)) return FILTER_SKIP;
    if (walker._filterFn === null) return FILTER_ACCEPT;
    walker._active = true;
    var result;
    try {
      result = walker._filterFn(node);
    } finally {
      walker._active = false;
    }
    return result;
  }

  // ── TreeWalker ───────────────────────────────────────────────────────
  // Ported from jsdom's TreeWalker-impl.js — see header comment.

  function TreeWalker(root, whatToShow, filter) {
    this.root = root;
    this.whatToShow = whatToShow === undefined ? NodeFilter.SHOW_ALL : (whatToShow >>> 0);
    this.filter = filter === undefined ? null : filter;
    this._filterFn = normalizeFilter(filter);
    this._active = false;
    this._currentNode = root;
  }
  Object.defineProperty(TreeWalker.prototype, 'currentNode', {
    get: function() { return this._currentNode; },
    set: function(node) {
      if (node === null || node === undefined) {
        throw new DOMException('Cannot set currentNode to null', 'NotSupportedError');
      }
      this._currentNode = node;
    },
    enumerable: true,
  });

  TreeWalker.prototype.parentNode = function() {
    var node = this._currentNode;
    while (node !== null && node !== this.root) {
      node = node.parentNode;
      if (node !== null && applyFilter(this, node) === FILTER_ACCEPT) {
        this._currentNode = node;
        return node;
      }
    }
    return null;
  };

  function traverseChildren(walker, forward) {
    var node = walker._currentNode;
    node = forward ? node.firstChild : node.lastChild;
    if (node === null) return null;

    mainLoop:
    for (;;) {
      var result = applyFilter(walker, node);
      if (result === FILTER_ACCEPT) {
        walker._currentNode = node;
        return node;
      }
      if (result === FILTER_SKIP) {
        var child = forward ? node.firstChild : node.lastChild;
        if (child !== null) { node = child; continue mainLoop; }
      }
      for (;;) {
        var sibling = forward ? node.nextSibling : node.previousSibling;
        if (sibling !== null) { node = sibling; continue mainLoop; }
        var parent = node.parentNode;
        if (parent === null || parent === walker.root || parent === walker._currentNode) return null;
        node = parent;
      }
    }
  }
  TreeWalker.prototype.firstChild = function() { return traverseChildren(this, true); };
  TreeWalker.prototype.lastChild = function() { return traverseChildren(this, false); };

  function traverseSiblings(walker, forward) {
    var node = walker._currentNode;
    if (node === walker.root) return null;

    for (;;) {
      var sibling = forward ? node.nextSibling : node.previousSibling;
      while (sibling !== null) {
        node = sibling;
        var result = applyFilter(walker, node);
        if (result === FILTER_ACCEPT) {
          walker._currentNode = node;
          return node;
        }
        sibling = forward ? node.firstChild : node.lastChild;
        if (result === FILTER_REJECT || sibling === null) {
          sibling = forward ? node.nextSibling : node.previousSibling;
        }
      }
      node = node.parentNode;
      if (node === null || node === walker.root) return null;
      if (applyFilter(walker, node) === FILTER_ACCEPT) return null;
    }
  }
  TreeWalker.prototype.previousSibling = function() { return traverseSiblings(this, false); };
  TreeWalker.prototype.nextSibling = function() { return traverseSiblings(this, true); };

  TreeWalker.prototype.previousNode = function() {
    var node = this._currentNode;
    while (node !== this.root) {
      var sibling = node.previousSibling;
      while (sibling !== null) {
        node = sibling;
        var result = applyFilter(this, node);
        while (result !== FILTER_REJECT && node.firstChild !== null) {
          node = node.lastChild;
          result = applyFilter(this, node);
        }
        if (result === FILTER_ACCEPT) {
          this._currentNode = node;
          return node;
        }
        sibling = node.previousSibling;
      }
      if (node === this.root || node.parentNode === null) return null;
      node = node.parentNode;
      if (applyFilter(this, node) === FILTER_ACCEPT) {
        this._currentNode = node;
        return node;
      }
    }
    return null;
  };

  TreeWalker.prototype.nextNode = function() {
    var node = this._currentNode;
    var result = FILTER_ACCEPT;
    for (;;) {
      while (result !== FILTER_REJECT && node.firstChild !== null) {
        node = node.firstChild;
        result = applyFilter(this, node);
        if (result === FILTER_ACCEPT) {
          this._currentNode = node;
          return node;
        }
      }
      var sibling = null;
      do {
        if (node === this.root) return null;
        sibling = node.nextSibling;
        if (sibling !== null) { node = sibling; break; }
        node = node.parentNode;
      } while (node !== null);
      if (node === null) return null;
      result = applyFilter(this, node);
      if (result === FILTER_ACCEPT) {
        this._currentNode = node;
        return node;
      }
    }
  };

  globalThis.TreeWalker = TreeWalker;

  // ── NodeIterator ─────────────────────────────────────────────────────
  // Ported from jsdom's NodeIterator-impl.js, with domSymbolTree.following()/
  // preceding() replaced by direct tree-order walks over the same node
  // properties TreeWalker uses.

  function followingNode(node, root) {
    if (node.firstChild !== null) return node.firstChild;
    var cur = node;
    while (cur !== null && cur !== root) {
      if (cur.nextSibling !== null) return cur.nextSibling;
      cur = cur.parentNode;
    }
    return null;
  }
  function lastInclusiveDescendant(node) {
    while (node.lastChild !== null) node = node.lastChild;
    return node;
  }
  function precedingNode(node, root) {
    if (node === root) return null;
    var sibling = node.previousSibling;
    if (sibling !== null) return lastInclusiveDescendant(sibling);
    return node.parentNode;
  }

  function NodeIterator(root, whatToShow, filter) {
    this.root = root;
    this.whatToShow = whatToShow === undefined ? NodeFilter.SHOW_ALL : (whatToShow >>> 0);
    this.filter = filter === undefined ? null : filter;
    this._filterFn = normalizeFilter(filter);
    this._active = false;
    this._referenceNode = root;
    this._pointerBeforeReferenceNode = true;
  }
  Object.defineProperty(NodeIterator.prototype, 'referenceNode', {
    get: function() { return this._referenceNode; },
    enumerable: true,
  });
  Object.defineProperty(NodeIterator.prototype, 'pointerBeforeReferenceNode', {
    get: function() { return this._pointerBeforeReferenceNode; },
    enumerable: true,
  });

  function iteratorTraverse(it, forward) {
    var node = it._referenceNode;
    var beforeNode = it._pointerBeforeReferenceNode;
    for (;;) {
      if (forward) {
        if (!beforeNode) {
          node = followingNode(node, it.root);
          if (node === null) return null;
        }
        beforeNode = false;
      } else {
        if (beforeNode) {
          node = precedingNode(node, it.root);
          if (node === null) return null;
        }
        beforeNode = true;
      }
      if (applyFilter(it, node) === FILTER_ACCEPT) break;
    }
    it._referenceNode = node;
    it._pointerBeforeReferenceNode = beforeNode;
    return node;
  }

  NodeIterator.prototype.nextNode = function() { return iteratorTraverse(this, true); };
  NodeIterator.prototype.previousNode = function() { return iteratorTraverse(this, false); };
  NodeIterator.prototype.detach = function() {};

  globalThis.NodeIterator = NodeIterator;

  // ── document.createTreeWalker / createNodeIterator ──────────────────
  document.createTreeWalker = function(root, whatToShow, filter) {
    if (!(root instanceof Node)) {
      throw new TypeError("Failed to execute 'createTreeWalker' on 'Document': parameter 1 is not of type 'Node'.");
    }
    return new TreeWalker(root, whatToShow, filter);
  };
  document.createNodeIterator = function(root, whatToShow, filter) {
    if (!(root instanceof Node)) {
      throw new TypeError("Failed to execute 'createNodeIterator' on 'Document': parameter 1 is not of type 'Node'.");
    }
    return new NodeIterator(root, whatToShow, filter);
  };
})();
)JS";

} // namespace

void TreeWalkerBindings::install(JSContext* ctx) {
  JSValue result = JS_Eval(ctx, kTreeWalkerBootstrapScript, strlen(kTreeWalkerBootstrapScript),
                            "<tree-walker-bootstrap>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JS_FreeValue(ctx, JS_GetException(ctx));
  }
  JS_FreeValue(ctx, result);
}

} // namespace margelo::nitro::nitrojsdom
