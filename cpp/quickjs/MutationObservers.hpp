#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>

// Forward-declare the opaque JSContext type to avoid including quickjs.h here
typedef struct JSContext JSContext;

namespace margelo::nitro::nitrojsdom {

// ── MutationRecord ────────────────────────────────────────────────────────────

struct MutationRecord {
  std::string type;                    // "childList" | "attributes" | "characterData"
  void* target { nullptr };           // lxb_dom_node_t*
  std::vector<void*> addedNodes;      // lxb_dom_node_t* list
  std::vector<void*> removedNodes;    // lxb_dom_node_t* list
  void* previousSibling { nullptr };  // lxb_dom_node_t*
  void* nextSibling { nullptr };      // lxb_dom_node_t*
  std::string attributeName;
  std::optional<std::string> oldValue;
};

// ── ObserverInit ──────────────────────────────────────────────────────────────

struct ObserverInit {
  bool childList { false };
  bool attributes { false };
  bool characterData { false };
  bool subtree { false };
  bool attributeOldValue { false };
  bool characterDataOldValue { false };
  std::optional<std::vector<std::string>> attributeFilter;
};

// ── RegisteredObserver ────────────────────────────────────────────────────────

struct RegisteredObserver {
  uint32_t id;
  void* target { nullptr };           // lxb_dom_node_t*
  ObserverInit options;
  void* js_callback { nullptr };      // JSValue* (heap-allocated, owned)
  std::vector<MutationRecord> queue;
  bool dispatch_scheduled { false };
  bool disconnected { false };
};

// ── MutationObservers ─────────────────────────────────────────────────────────

class MutationObservers {
public:
  MutationObservers() = default;
  ~MutationObservers() = default;

  // Called from DOMBindings::install to register the MutationObserver constructor
  void install(JSContext* ctx, void* doc_root);

  // Register an observer; returns its id
  uint32_t addObserver(void* target, ObserverInit options, void* js_callback);

  // Remove an observer and discard pending records
  void disconnect(uint32_t observer_id);

  // Flush and return pending records (without invoking callback)
  std::vector<MutationRecord> takeRecords(uint32_t observer_id);

  // Mutation notification hooks (called from DOMBindings after each mutation)
  void notifyChildList(JSContext* ctx, void* target, std::vector<void*> added,
                       std::vector<void*> removed, void* prev_sibling, void* next_sibling);
  void notifyAttribute(JSContext* ctx, void* target, const std::string& attr_name,
                       std::optional<std::string> old_value);
  void notifyCharacterData(JSContext* ctx, void* target, std::optional<std::string> old_value);

  // True when no active (non-disconnected) observers exist (fast-path check)
  bool empty() const { return _active_count == 0; }

  // True if at least one active observer requests attributeOldValue capture
  bool hasAttributeOldValueObserver() const {
    for (const auto& obs : _observers)
      if (!obs.disconnected && obs.options.attributeOldValue) return true;
    return false;
  }

  // True if at least one active observer requests characterDataOldValue capture
  bool hasCharacterDataOldValueObserver() const {
    for (const auto& obs : _observers)
      if (!obs.disconnected && obs.options.characterDataOldValue) return true;
    return false;
  }

  // Free all held JSValue callbacks — must be called before JS_FreeContext
  void clearAll(JSContext* ctx);

  // Store the document root pointer for ancestry checks
  void setDocumentRoot(void* doc_root) { _doc_root = doc_root; }

  // Update target + options for an existing observer (called from observe())
  void updateObserver(uint32_t id, void* target, ObserverInit options);

  // Disconnect any observer whose target is in the given set of destroyed nodes
  void disconnectDetachedTargets(const std::vector<void*>& destroyed_nodes);

  // Public wrappers around private helpers (needed by dispatch_trampoline)
  bool isInDocument_public(void* node) const;
  std::vector<RegisteredObserver>& _observers_ref();

private:
  std::vector<RegisteredObserver> _observers;
  uint32_t _next_id { 1 };
  uint32_t _active_count { 0 };  // number of non-disconnected observers
  void* _doc_root { nullptr };

  // Returns true if node is still attached to _doc_root's tree
  bool isInDocument(void* node) const;

  // Returns true if ancestor is an ancestor-or-equal of node
  bool isAncestorOf(void* ancestor, void* node) const;

  // Schedule a microtask dispatch for the observer if not already scheduled
  void scheduleDispatch(JSContext* ctx, RegisteredObserver& observer);
};

} // namespace margelo::nitro::nitrojsdom
