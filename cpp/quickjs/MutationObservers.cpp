#include "MutationObservers.hpp"
#include "DOMBindingsInternal.hpp"
#include "QuickJSRuntime.hpp"
#include "quickjs.h"
#include <lexbor/dom/dom.h>
#include <algorithm>

namespace margelo::nitro::nitrojsdom {

// ── Forward declarations ───────────────────────────────────────────────────────

struct MutationObserverOpaque;
struct DispatchJob;

// ── JS Class IDs ──────────────────────────────────────────────────────────────

static JSClassID js_mutation_observer_class_id = 0;
static JSClassID js_dispatch_carrier_class_id  = 0;

static void mo_finalizer(JSRuntime*, JSValue val) {
  auto* opaque = static_cast<MutationObserverOpaque*>(
      JS_GetOpaque(val, js_mutation_observer_class_id));
  delete opaque;
}

static void carrier_finalizer(JSRuntime*, JSValue val) {
  auto* job = static_cast<DispatchJob*>(JS_GetOpaque(val, js_dispatch_carrier_class_id));
  delete job;
}

static JSClassDef js_mutation_observer_class = { "MutationObserver", .finalizer = mo_finalizer };
static JSClassDef js_dispatch_carrier_class  = { "_DispatchCarrier", .finalizer = carrier_finalizer };

// ── MutationObserver opaque data ──────────────────────────────────────────────

struct MutationObserverOpaque {
  MutationObservers* registry { nullptr };
  uint32_t id { 0 };
};

// ── Dispatch trampoline (passed to JS_EnqueueJob) ─────────────────────────────

// The job data: a heap-allocated pair of (registry*, observer_id).
// We can't store a RegisteredObserver* because the vector may reallocate.
struct DispatchJob {
  MutationObservers* registry { nullptr };
  uint32_t observer_id { 0 };
  uint32_t generation { 0 };  // BUG-6: generation at time of scheduling
};

static JSValue dispatch_trampoline(JSContext* ctx, int argc, JSValue* argv) {
  (void)argc; (void)argv;
  // argv[0] holds a pointer to DispatchJob encoded as a carrier opaque object
  DispatchJob* job = static_cast<DispatchJob*>(JS_GetOpaque(argv[0], js_dispatch_carrier_class_id));
  if (!job) return JS_UNDEFINED;

  MutationObservers* reg = job->registry;
  uint32_t obs_id = job->observer_id;

  // Find the observer
  RegisteredObserver* obs = nullptr;
  for (auto& o : reg->_observers_ref()) {
    if (o.id == obs_id) { obs = &o; break; }
  }

  // BUG-6: check generation to detect stale jobs enqueued before takeRecords()
  if (!obs || obs->disconnected || obs->queue.empty() ||
      obs->dispatch_generation != job->generation) {
    if (obs) obs->dispatch_scheduled = false;
    return JS_UNDEFINED;
  }

  // Mark as no longer scheduled BEFORE calling callback (so re-entry works)
  obs->dispatch_scheduled = false;

  // Drain and materialize the record queue
  auto records = std::move(obs->queue);
  obs->queue.clear();

  if (!obs->js_callback) return JS_UNDEFINED;
  JSValue* cb = static_cast<JSValue*>(obs->js_callback);

  // Build JS array of MutationRecord objects
  JSValue arr = JS_NewArray(ctx);
  for (size_t i = 0; i < records.size(); i++) {
    auto& r = records[i];

    // Skip records whose target node is no longer in the document
    if (!reg->isInDocument_public(r.target)) continue;

    JSValue rec_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rec_obj, "type",   JS_NewString(ctx, r.type.c_str()));
    JS_SetPropertyStr(ctx, rec_obj, "target", make_element(ctx, r.target));

    // addedNodes array
    JSValue added = JS_NewArray(ctx);
    for (size_t j = 0; j < r.addedNodes.size(); j++)
      JS_SetPropertyUint32(ctx, added, (uint32_t)j, make_element(ctx, r.addedNodes[j]));
    JS_SetPropertyStr(ctx, rec_obj, "addedNodes", added);

    // removedNodes array
    JSValue removed = JS_NewArray(ctx);
    for (size_t j = 0; j < r.removedNodes.size(); j++)
      JS_SetPropertyUint32(ctx, removed, (uint32_t)j, make_element(ctx, r.removedNodes[j]));
    JS_SetPropertyStr(ctx, rec_obj, "removedNodes", removed);

    JS_SetPropertyStr(ctx, rec_obj, "previousSibling",
        r.previousSibling ? make_element(ctx, r.previousSibling) : JS_NULL);
    JS_SetPropertyStr(ctx, rec_obj, "nextSibling",
        r.nextSibling ? make_element(ctx, r.nextSibling) : JS_NULL);
    JS_SetPropertyStr(ctx, rec_obj, "attributeName",
        r.attributeName.empty() ? JS_NULL : JS_NewString(ctx, r.attributeName.c_str()));
    JS_SetPropertyStr(ctx, rec_obj, "oldValue",
        r.oldValue.has_value() ? JS_NewString(ctx, r.oldValue->c_str()) : JS_NULL);

    JS_SetPropertyUint32(ctx, arr, (uint32_t)i, rec_obj);
  }

  // Call the user callback: cb([records])
  JSValue ret = JS_Call(ctx, *cb, JS_UNDEFINED, 1, &arr);
  JS_FreeValue(ctx, arr);

  if (JS_IsException(ret)) {
    // Route exception as unhandled rejection via RuntimeContext pending_rejection
    RuntimeContext* rctx = static_cast<RuntimeContext*>(JS_GetContextOpaque(ctx));
    if (rctx) {
      if (rctx->pending_rejection) {
        JSValue* old = static_cast<JSValue*>(rctx->pending_rejection);
        JS_FreeValue(ctx, *old);
        delete old;
        rctx->pending_rejection = nullptr;
      }
      JSValue ex = JS_GetException(ctx);
      rctx->pending_rejection = new JSValue(JS_DupValue(ctx, ex));
      JS_FreeValue(ctx, ex);
    }
    JS_FreeValue(ctx, ret);
    return JS_UNDEFINED;
  }
  JS_FreeValue(ctx, ret);
  return JS_UNDEFINED;
}

// ── JS MutationObserver constructor ──────────────────────────────────────────

static JSValue js_MutationObserver_constructor(JSContext* ctx, JSValue new_target,
                                               int argc, JSValue* argv) {
  (void)new_target;
  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    JS_ThrowTypeError(ctx, "MutationObserver requires a callback function");
    return JS_EXCEPTION;
  }

  RuntimeContext* rctx = static_cast<RuntimeContext*>(JS_GetContextOpaque(ctx));
  if (!rctx || !rctx->mutation_observers) return JS_UNDEFINED;

  // Dup the callback and store in the registry (target set later via observe())
  void* cb = new JSValue(JS_DupValue(ctx, argv[0]));
  uint32_t id = rctx->mutation_observers->addObserver(nullptr, ObserverInit{}, cb);

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_mutation_observer_class_id);
  auto* opaque = new MutationObserverOpaque{ rctx->mutation_observers.get(), id };
  JS_SetOpaque(obj, opaque);

  return obj;
}

// ── JS MutationObserver.observe(target, options) ──────────────────────────────

static JSValue js_MutationObserver_observe(JSContext* ctx, JSValue this_val,
                                            int argc, JSValue* argv) {
  auto* opaque = static_cast<MutationObserverOpaque*>(
      JS_GetOpaque(this_val, js_mutation_observer_class_id));
  if (!opaque || !opaque->registry) return JS_UNDEFINED;
  if (argc < 1) return JS_UNDEFINED;

  // Resolve target node
  void* target = JS_GetOpaque(argv[0], js_element_class_id);
  if (!target) return JS_UNDEFINED; // not an element — no-op

  ObserverInit opts;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue v;
    auto getBool = [&](const char* name) -> bool {
      v = JS_GetPropertyStr(ctx, argv[1], name);
      bool r = JS_ToBool(ctx, v);
      JS_FreeValue(ctx, v);
      return r;
    };
    opts.childList          = getBool("childList");
    opts.attributes         = getBool("attributes");
    opts.characterData      = getBool("characterData");
    opts.subtree            = getBool("subtree");
    opts.attributeOldValue  = getBool("attributeOldValue");
    opts.characterDataOldValue = getBool("characterDataOldValue");

    // attributeFilter
    JSValue filterVal = JS_GetPropertyStr(ctx, argv[1], "attributeFilter");
    if (JS_IsArray(ctx, filterVal)) {
      std::vector<std::string> filters;
      JSValue lenVal = JS_GetPropertyStr(ctx, filterVal, "length");
      int32_t len = 0;
      JS_ToInt32(ctx, &len, lenVal);
      JS_FreeValue(ctx, lenVal);
      for (int32_t i = 0; i < len; i++) {
        JSValue item = JS_GetPropertyUint32(ctx, filterVal, (uint32_t)i);
        const char* s = JS_ToCString(ctx, item);
        if (s) { filters.push_back(s); JS_FreeCString(ctx, s); }
        JS_FreeValue(ctx, item);
      }
      opts.attributeFilter = std::move(filters);
    }
    JS_FreeValue(ctx, filterVal);
  }

  // GAP-3: per WHATWG spec, attributeFilter or attributeOldValue implicitly
  // enables attributes observation; characterDataOldValue implies characterData.
  if ((opts.attributeFilter.has_value() || opts.attributeOldValue) && !opts.attributes)
    opts.attributes = true;
  if (opts.characterDataOldValue && !opts.characterData)
    opts.characterData = true;

  // GAP-4: per WHATWG spec, observe() must throw TypeError if none of the
  // three mutation types is enabled after applying the implicit-enable rules.
  if (!opts.childList && !opts.attributes && !opts.characterData) {
    JS_ThrowTypeError(ctx,
        "The options object must set at least one of 'attributes', 'characterData', or 'childList' to true.");
    return JS_EXCEPTION;
  }

  // Update the existing observer with the new target and options
  opaque->registry->updateObserver(opaque->id, reinterpret_cast<lxb_dom_node_t*>(target), opts);
  return JS_UNDEFINED;
}

// ── JS MutationObserver.disconnect() ─────────────────────────────────────────

static JSValue js_MutationObserver_disconnect(JSContext* ctx, JSValue this_val,
                                               int, JSValue*) {
  (void)ctx;
  auto* opaque = static_cast<MutationObserverOpaque*>(
      JS_GetOpaque(this_val, js_mutation_observer_class_id));
  if (!opaque || !opaque->registry) return JS_UNDEFINED;
  opaque->registry->disconnect(opaque->id);
  return JS_UNDEFINED;
}

// ── JS MutationObserver.takeRecords() ────────────────────────────────────────

static JSValue js_MutationObserver_takeRecords(JSContext* ctx, JSValue this_val,
                                                int, JSValue*) {
  auto* opaque = static_cast<MutationObserverOpaque*>(
      JS_GetOpaque(this_val, js_mutation_observer_class_id));
  if (!opaque || !opaque->registry) return JS_NewArray(ctx);

  auto records = opaque->registry->takeRecords(opaque->id);
  JSValue arr = JS_NewArray(ctx);
  for (size_t i = 0; i < records.size(); i++) {
    auto& r = records[i];
    JSValue rec_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rec_obj, "type",   JS_NewString(ctx, r.type.c_str()));
    JS_SetPropertyStr(ctx, rec_obj, "target", make_element(ctx, r.target));

    JSValue added = JS_NewArray(ctx);
    for (size_t j = 0; j < r.addedNodes.size(); j++)
      JS_SetPropertyUint32(ctx, added, (uint32_t)j, make_element(ctx, r.addedNodes[j]));
    JS_SetPropertyStr(ctx, rec_obj, "addedNodes", added);

    JSValue removed = JS_NewArray(ctx);
    for (size_t j = 0; j < r.removedNodes.size(); j++)
      JS_SetPropertyUint32(ctx, removed, (uint32_t)j, make_element(ctx, r.removedNodes[j]));
    JS_SetPropertyStr(ctx, rec_obj, "removedNodes", removed);

    JS_SetPropertyStr(ctx, rec_obj, "attributeName",
        r.attributeName.empty() ? JS_NULL : JS_NewString(ctx, r.attributeName.c_str()));
    JS_SetPropertyStr(ctx, rec_obj, "oldValue",
        r.oldValue.has_value() ? JS_NewString(ctx, r.oldValue->c_str()) : JS_NULL);

    JS_SetPropertyUint32(ctx, arr, (uint32_t)i, rec_obj);
  }
  return arr;
}

// ── MutationObservers::install ────────────────────────────────────────────────

void MutationObservers::install(JSContext* ctx, void* doc_root) {
  _doc_root = doc_root;

  // Register JS classes
  JS_NewClassID(&js_mutation_observer_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_mutation_observer_class_id, &js_mutation_observer_class);
  JS_NewClassID(&js_dispatch_carrier_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_dispatch_carrier_class_id, &js_dispatch_carrier_class);

  // Build prototype
  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, proto, "observe",     JS_NewCFunction(ctx, js_MutationObserver_observe,     "observe",     2));
  JS_SetPropertyStr(ctx, proto, "disconnect",  JS_NewCFunction(ctx, js_MutationObserver_disconnect,  "disconnect",  0));
  JS_SetPropertyStr(ctx, proto, "takeRecords", JS_NewCFunction(ctx, js_MutationObserver_takeRecords, "takeRecords", 0));
  JS_SetClassProto(ctx, js_mutation_observer_class_id, proto);

  // Register constructor on global
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue ctor = JS_NewCFunction2(ctx, js_MutationObserver_constructor,
                                  "MutationObserver", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, global, "MutationObserver", ctor);
  JS_FreeValue(ctx, global);
}

// ── MutationObservers::addObserver ────────────────────────────────────────────

uint32_t MutationObservers::addObserver(void* target, ObserverInit options, void* js_callback) {
  RegisteredObserver obs;
  obs.id = _next_id++;
  obs.target = target;
  obs.options = std::move(options);
  obs.js_callback = js_callback;
  obs.dispatch_scheduled = false;
  obs.disconnected = false;
  _observers.push_back(std::move(obs));
  _active_count++;
  return _observers.back().id;
}

// ── MutationObservers::updateObserver ────────────────────────────────────────

void MutationObservers::updateObserver(uint32_t id, void* target, ObserverInit options) {
  for (auto& obs : _observers) {
    if (obs.id == id) {
      if (obs.disconnected) {
        // Re-activating a previously disconnected observer
        obs.disconnected = false;
        _active_count++;
      }
      obs.target = target;
      obs.options = std::move(options);
      // EDGE-3: per WHATWG spec, re-observing the same target must NOT discard
      // pending records in the queue — only the options are replaced.
      return;
    }
  }
}

// ── MutationObservers::disconnect ────────────────────────────────────────────

void MutationObservers::disconnect(uint32_t observer_id) {
  for (auto& obs : _observers) {
    if (obs.id == observer_id) {
      if (!obs.disconnected) {
        obs.disconnected = true;
        if (_active_count > 0) _active_count--;
      }
      obs.queue.clear();
      obs.dispatch_scheduled = false;
      return;
    }
  }
}

// ── MutationObservers::takeRecords ───────────────────────────────────────────

std::vector<MutationRecord> MutationObservers::takeRecords(uint32_t observer_id) {
  for (auto& obs : _observers) {
    if (obs.id == observer_id) {
      auto records = std::move(obs.queue);
      obs.queue.clear();
      obs.dispatch_scheduled = false;
      // BUG-6: bump generation so any already-enqueued job in the QuickJS
      // job queue is recognized as stale and skipped in dispatch_trampoline.
      obs.dispatch_generation++;
      return records;
    }
  }
  return {};
}

// ── Ancestry helpers ──────────────────────────────────────────────────────────

bool MutationObservers::isInDocument(void* node_ptr) const {
  if (!node_ptr || !_doc_root) return false;
  lxb_dom_node_t* node = static_cast<lxb_dom_node_t*>(node_ptr);
  lxb_dom_node_t* walk = node;
  while (walk) {
    if (walk == static_cast<lxb_dom_node_t*>(_doc_root)) return true;
    walk = walk->parent;
  }
  return false;
}

bool MutationObservers::isInDocument_public(void* node_ptr) const {
  return isInDocument(node_ptr);
}

bool MutationObservers::isAncestorOf(void* ancestor_ptr, void* node_ptr) const {
  if (!ancestor_ptr || !node_ptr) return false;
  lxb_dom_node_t* ancestor = static_cast<lxb_dom_node_t*>(ancestor_ptr);
  lxb_dom_node_t* node = static_cast<lxb_dom_node_t*>(node_ptr);
  lxb_dom_node_t* walk = node;
  while (walk) {
    if (walk == ancestor) return true;
    walk = walk->parent;
  }
  return false;
}

// ── MutationObservers::scheduleDispatch ──────────────────────────────────────

void MutationObservers::scheduleDispatch(JSContext* ctx, RegisteredObserver& observer) {
  if (observer.dispatch_scheduled || observer.disconnected) return;
  observer.dispatch_scheduled = true;

  // Create an opaque carrier object for the trampoline
  JSValue carrier = JS_NewObjectClass(ctx, js_dispatch_carrier_class_id);
  // We store a DispatchJob as opaque — the carrier object owns it
  // BUG-6: include current generation so stale jobs can be ignored
  auto* job = new DispatchJob{ this, observer.id, observer.dispatch_generation };
  JS_SetOpaque(carrier, job);

  // Enqueue a job — the carrier JSValue is passed as argv[0]
  JS_EnqueueJob(ctx, dispatch_trampoline, 1, &carrier);
  JS_FreeValue(ctx, carrier);
}

// ── notifyChildList ───────────────────────────────────────────────────────────

void MutationObservers::notifyChildList(JSContext* ctx, void* target,
                                         std::vector<void*> added,
                                         std::vector<void*> removed,
                                         void* prev_sibling, void* next_sibling) {
  if (_observers.empty()) return;

  for (auto& obs : _observers) {
    if (obs.disconnected || !obs.options.childList) continue;
    if (!obs.target) continue;

    bool matches = (obs.target == target) ||
                   (obs.options.subtree && isAncestorOf(obs.target, target));
    if (!matches) continue;

    MutationRecord rec;
    rec.type = "childList";
    rec.target = target;
    rec.addedNodes = added;
    rec.removedNodes = removed;
    rec.previousSibling = prev_sibling;
    rec.nextSibling = next_sibling;
    obs.queue.push_back(std::move(rec));
    scheduleDispatch(ctx, obs);
  }
}

// ── notifyAttribute ───────────────────────────────────────────────────────────

void MutationObservers::notifyAttribute(JSContext* ctx, void* target,
                                         const std::string& attr_name,
                                         std::optional<std::string> old_value) {
  if (_observers.empty()) return;

  for (auto& obs : _observers) {
    if (obs.disconnected || !obs.options.attributes) continue;
    if (!obs.target) continue;

    bool matches = (obs.target == target) ||
                   (obs.options.subtree && isAncestorOf(obs.target, target));
    if (!matches) continue;

    // Check attributeFilter
    if (obs.options.attributeFilter.has_value()) {
      const auto& filters = obs.options.attributeFilter.value();
      bool allowed = std::find(filters.begin(), filters.end(), attr_name) != filters.end();
      if (!allowed) continue;
    }

    MutationRecord rec;
    rec.type = "attributes";
    rec.target = target;
    rec.attributeName = attr_name;
    rec.oldValue = obs.options.attributeOldValue ? old_value : std::nullopt;
    obs.queue.push_back(std::move(rec));
    scheduleDispatch(ctx, obs);
  }
}

// ── notifyCharacterData ───────────────────────────────────────────────────────

void MutationObservers::notifyCharacterData(JSContext* ctx, void* target,
                                             std::optional<std::string> old_value) {
  if (_observers.empty()) return;

  for (auto& obs : _observers) {
    if (obs.disconnected || !obs.options.characterData) continue;
    if (!obs.target) continue;

    bool matches = (obs.target == target) ||
                   (obs.options.subtree && isAncestorOf(obs.target, target));
    if (!matches) continue;

    MutationRecord rec;
    rec.type = "characterData";
    rec.target = target;
    rec.oldValue = obs.options.characterDataOldValue ? old_value : std::nullopt;
    obs.queue.push_back(std::move(rec));
    scheduleDispatch(ctx, obs);
  }
}

// ── MutationObservers::disconnectDetachedTargets ──────────────────────────────

void MutationObservers::disconnectDetachedTargets(const std::vector<void*>& /*destroyed_nodes*/) {
  // EDGE-2 fix: instead of checking only direct equality with destroyed_nodes
  // (which misses subtree descendants), use isInDocument to check every active
  // observer's target. Any target no longer reachable from the document root
  // is disconnected, which covers subtrees of any depth.
  if (_observers.empty()) return;
  for (auto& obs : _observers) {
    if (obs.disconnected || !obs.target) continue;
    if (!isInDocument(obs.target)) {
      obs.disconnected = true;
      if (_active_count > 0) _active_count--;
      obs.dispatch_scheduled = false;
      // BUG-3 fix: also purge removedNodes from queued records to prevent UAF
      // when the trampoline later tries to materialize them.
      for (auto& rec : obs.queue) {
        rec.removedNodes.clear();
      }
      obs.queue.clear();
    }
  }
  // BUG-3 fix: for observers whose targets ARE still in the document,
  // purge any removedNodes references that are now detached (dangling).
  for (auto& obs : _observers) {
    if (obs.disconnected) continue;
    for (auto& rec : obs.queue) {
      if (!rec.removedNodes.empty()) {
        rec.removedNodes.erase(
          std::remove_if(rec.removedNodes.begin(), rec.removedNodes.end(),
            [this](void* n) { return !isInDocument(n); }),
          rec.removedNodes.end());
      }
    }
  }
}

// ── MutationObservers::clearAll ───────────────────────────────────────────────

void MutationObservers::clearAll(JSContext* ctx) {
  for (auto& obs : _observers) {
    if (obs.js_callback) {
      JSValue* cb = static_cast<JSValue*>(obs.js_callback);
      JS_FreeValue(ctx, *cb);
      delete cb;
      obs.js_callback = nullptr;
    }
    obs.queue.clear();
  }
  _observers.clear();
  _active_count = 0;
}

// ── _observers_ref (used by trampoline) ──────────────────────────────────────

std::vector<RegisteredObserver>& MutationObservers::_observers_ref() {
  return _observers;
}

} // namespace margelo::nitro::nitrojsdom
