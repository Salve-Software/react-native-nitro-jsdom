#include "EventBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include "../QuickJSRuntime.hpp"
#include "../../lexbor/LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <string>
#include <vector>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_event_class_id = 0;
JSClassDef js_event_class = { "Event", .finalizer = nullptr };

// ── Event / CustomEvent constructors ──────────────────────────────────────────

JSValue js_Event_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type_str ? type_str : ""));
  if (type_str) JS_FreeCString(ctx, type_str);

  bool bubbles = false;
  bool cancelable = false;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue b = JS_GetPropertyStr(ctx, argv[1], "bubbles");
    if (JS_IsException(b)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { bubbles = JS_ToBool(ctx, b) > 0; JS_FreeValue(ctx, b); }

    JSValue c = JS_GetPropertyStr(ctx, argv[1], "cancelable");
    if (JS_IsException(c)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { cancelable = JS_ToBool(ctx, c) > 0; JS_FreeValue(ctx, c); }
  }

  bool composed = false;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue c = JS_GetPropertyStr(ctx, argv[1], "composed");
    if (JS_IsException(c)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { composed = JS_ToBool(ctx, c) > 0; JS_FreeValue(ctx, c); }
  }

  JS_SetPropertyStr(ctx, obj, "defaultPrevented",  JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "bubbles",           JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable",        JS_NewBool(ctx, cancelable));
  JS_SetPropertyStr(ctx, obj, "composed",          JS_NewBool(ctx, composed));
  return obj;
}

JSValue js_CustomEvent_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type_str ? type_str : ""));
  if (type_str) JS_FreeCString(ctx, type_str);

  JSValue detail = JS_UNDEFINED;
  bool bubbles = false;
  bool cancelable = false;

  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue d = JS_GetPropertyStr(ctx, argv[1], "detail");
    if (JS_IsException(d)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { detail = d; }

    JSValue b = JS_GetPropertyStr(ctx, argv[1], "bubbles");
    if (JS_IsException(b)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { bubbles = JS_ToBool(ctx, b) > 0; JS_FreeValue(ctx, b); }

    JSValue c = JS_GetPropertyStr(ctx, argv[1], "cancelable");
    if (JS_IsException(c)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { cancelable = JS_ToBool(ctx, c) > 0; JS_FreeValue(ctx, c); }
  }

  bool composed = false;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue c = JS_GetPropertyStr(ctx, argv[1], "composed");
    if (JS_IsException(c)) { JS_FreeValue(ctx, JS_GetException(ctx)); }
    else { composed = JS_ToBool(ctx, c) > 0; JS_FreeValue(ctx, c); }
  }

  JS_SetPropertyStr(ctx, obj, "detail",           detail);
  JS_SetPropertyStr(ctx, obj, "bubbles",          JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable",       JS_NewBool(ctx, cancelable));
  JS_SetPropertyStr(ctx, obj, "composed",         JS_NewBool(ctx, composed));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented", JS_NewBool(ctx, false));
  return obj;
}

// Reads a numeric/string init-dict field, falling back to `def` if the field
// is absent or the dict itself wasn't passed — shared by KeyboardEvent and
// MouseEvent, whose constructors otherwise differ only in which fields they copy.
double get_num_init_field(JSContext* ctx, JSValue init, const char* name, double def) {
  if (!JS_IsObject(init)) return def;
  JSValue v = JS_GetPropertyStr(ctx, init, name);
  if (JS_IsException(v)) { JS_FreeValue(ctx, JS_GetException(ctx)); return def; }
  if (JS_IsUndefined(v)) { JS_FreeValue(ctx, v); return def; }
  double d = def;
  JS_ToFloat64(ctx, &d, v);
  JS_FreeValue(ctx, v);
  return d;
}

// get_bool_prop() (DOMBindingsInternal.hpp) assumes a real object receiver;
// init dicts are optional, so this guards the JS_IsObject check first.
bool get_bool_init_field(JSContext* ctx, JSValue init, const char* name) {
  if (!JS_IsObject(init)) return false;
  return get_bool_prop(ctx, init, name);
}

std::string get_str_init_field(JSContext* ctx, JSValue init, const char* name, const char* def) {
  if (!JS_IsObject(init)) return def;
  JSValue v = JS_GetPropertyStr(ctx, init, name);
  if (JS_IsException(v)) { JS_FreeValue(ctx, JS_GetException(ctx)); return def; }
  if (JS_IsUndefined(v)) { JS_FreeValue(ctx, v); return def; }
  const char* s = JS_ToCString(ctx, v);
  std::string result = s ? s : def;
  if (s) JS_FreeCString(ctx, s);
  JS_FreeValue(ctx, v);
  return result;
}

JSValue js_KeyboardEvent_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type_str ? type_str : ""));
  if (type_str) JS_FreeCString(ctx, type_str);

  JSValue init = (argc >= 2) ? argv[1] : JS_UNDEFINED;
  JS_SetPropertyStr(ctx, obj, "bubbles",    JS_NewBool(ctx, get_bool_init_field(ctx, init, "bubbles")));
  JS_SetPropertyStr(ctx, obj, "cancelable", JS_NewBool(ctx, get_bool_init_field(ctx, init, "cancelable")));
  JS_SetPropertyStr(ctx, obj, "composed",   JS_NewBool(ctx, get_bool_init_field(ctx, init, "composed")));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented", JS_NewBool(ctx, false));

  std::string key = get_str_init_field(ctx, init, "key", "");
  std::string code = get_str_init_field(ctx, init, "code", "");
  JS_SetPropertyStr(ctx, obj, "key",      JS_NewStringLen(ctx, key.c_str(), key.size()));
  JS_SetPropertyStr(ctx, obj, "code",     JS_NewStringLen(ctx, code.c_str(), code.size()));
  JS_SetPropertyStr(ctx, obj, "location", JS_NewInt32(ctx, (int32_t)get_num_init_field(ctx, init, "location", 0)));
  JS_SetPropertyStr(ctx, obj, "repeat",   JS_NewBool(ctx, get_bool_init_field(ctx, init, "repeat")));
  JS_SetPropertyStr(ctx, obj, "ctrlKey",  JS_NewBool(ctx, get_bool_init_field(ctx, init, "ctrlKey")));
  JS_SetPropertyStr(ctx, obj, "shiftKey", JS_NewBool(ctx, get_bool_init_field(ctx, init, "shiftKey")));
  JS_SetPropertyStr(ctx, obj, "altKey",   JS_NewBool(ctx, get_bool_init_field(ctx, init, "altKey")));
  JS_SetPropertyStr(ctx, obj, "metaKey",  JS_NewBool(ctx, get_bool_init_field(ctx, init, "metaKey")));
  return obj;
}

JSValue js_MouseEvent_constructor(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  const char* type_str = (argc >= 1) ? JS_ToCString(ctx, argv[0]) : nullptr;
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type_str ? type_str : ""));
  if (type_str) JS_FreeCString(ctx, type_str);

  JSValue init = (argc >= 2) ? argv[1] : JS_UNDEFINED;
  JS_SetPropertyStr(ctx, obj, "bubbles",    JS_NewBool(ctx, get_bool_init_field(ctx, init, "bubbles")));
  JS_SetPropertyStr(ctx, obj, "cancelable", JS_NewBool(ctx, get_bool_init_field(ctx, init, "cancelable")));
  JS_SetPropertyStr(ctx, obj, "composed",   JS_NewBool(ctx, get_bool_init_field(ctx, init, "composed")));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented", JS_NewBool(ctx, false));

  JS_SetPropertyStr(ctx, obj, "clientX", JS_NewFloat64(ctx, get_num_init_field(ctx, init, "clientX", 0)));
  JS_SetPropertyStr(ctx, obj, "clientY", JS_NewFloat64(ctx, get_num_init_field(ctx, init, "clientY", 0)));
  JS_SetPropertyStr(ctx, obj, "screenX", JS_NewFloat64(ctx, get_num_init_field(ctx, init, "screenX", 0)));
  JS_SetPropertyStr(ctx, obj, "screenY", JS_NewFloat64(ctx, get_num_init_field(ctx, init, "screenY", 0)));
  JS_SetPropertyStr(ctx, obj, "pageX",   JS_NewFloat64(ctx, get_num_init_field(ctx, init, "pageX", 0)));
  JS_SetPropertyStr(ctx, obj, "pageY",   JS_NewFloat64(ctx, get_num_init_field(ctx, init, "pageY", 0)));
  JS_SetPropertyStr(ctx, obj, "button",  JS_NewInt32(ctx, (int32_t)get_num_init_field(ctx, init, "button", 0)));
  JS_SetPropertyStr(ctx, obj, "buttons", JS_NewInt32(ctx, (int32_t)get_num_init_field(ctx, init, "buttons", 0)));
  JS_SetPropertyStr(ctx, obj, "ctrlKey",  JS_NewBool(ctx, get_bool_init_field(ctx, init, "ctrlKey")));
  JS_SetPropertyStr(ctx, obj, "shiftKey", JS_NewBool(ctx, get_bool_init_field(ctx, init, "shiftKey")));
  JS_SetPropertyStr(ctx, obj, "altKey",   JS_NewBool(ctx, get_bool_init_field(ctx, init, "altKey")));
  JS_SetPropertyStr(ctx, obj, "metaKey",  JS_NewBool(ctx, get_bool_init_field(ctx, init, "metaKey")));
  return obj;
}

// ── Event prototype methods ───────────────────────────────────────────────────

JSValue js_event_preventDefault(JSContext* ctx, JSValue this_val, int, JSValue*) {
  if (get_bool_prop(ctx, this_val, "cancelable"))
    JS_SetPropertyStr(ctx, this_val, "defaultPrevented", JS_NewBool(ctx, true));
  return JS_UNDEFINED;
}

JSValue js_event_stopPropagation(JSContext* ctx, JSValue this_val, int, JSValue*) {
  JS_SetPropertyStr(ctx, this_val, "__propagationStopped", JS_NewBool(ctx, true));
  return JS_UNDEFINED;
}

JSValue js_event_stopImmediatePropagation(JSContext* ctx, JSValue this_val, int, JSValue*) {
  JS_SetPropertyStr(ctx, this_val, "__propagationStopped", JS_NewBool(ctx, true));
  JS_SetPropertyStr(ctx, this_val, "__immediatePropagationStopped", JS_NewBool(ctx, true));
  return JS_UNDEFINED;
}

// __composedPath is populated by dispatch_event_on_target() right before
// listener invocation begins; an event that was never dispatched (just
// constructed) has none, matching the spec's "not currently dispatched" case.
JSValue js_event_composedPath(JSContext* ctx, JSValue this_val, int, JSValue*) {
  JSValue path = JS_GetPropertyStr(ctx, this_val, "__composedPath");
  if (JS_IsUndefined(path) || JS_IsException(path)) {
    if (JS_IsException(path)) JS_FreeValue(ctx, JS_GetException(ctx));
    return JS_NewArray(ctx);
  }
  return path;
}

// ── addEventListener / removeEventListener (element) ──────────────────────────

JSValue js_el_addEventListener(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  auto* node_ptr = unwrap_node(ctx, this_val);
  if (!node_ptr) return JS_UNDEFINED;
  auto* el = reinterpret_cast<lxb_dom_element_t*>(node_ptr);

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  void* node = lxb_dom_interface_node(el);
  for (auto& l : rctx->listeners) {
    if (l.node == node && l.event_type == event_type && !l.is_handler_property) {
      JSValue* stored = static_cast<JSValue*>(l.callback);
      if (JS_VALUE_GET_TAG(*stored) == JS_VALUE_GET_TAG(argv[1]) &&
          JS_VALUE_GET_PTR(*stored) == JS_VALUE_GET_PTR(argv[1]))
        return JS_UNDEFINED;
    }
  }

  EventListener listener;
  listener.node = node;
  listener.event_type = event_type;
  listener.callback = new JSValue(JS_DupValue(ctx, argv[1]));
  rctx->listeners.push_back(std::move(listener));
  return JS_UNDEFINED;
}

JSValue js_el_removeEventListener(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  auto* node_ptr = unwrap_node(ctx, this_val);
  if (!node_ptr) return JS_UNDEFINED;
  auto* el = reinterpret_cast<lxb_dom_element_t*>(node_ptr);

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  void* node = lxb_dom_interface_node(el);
  for (auto it = rctx->listeners.begin(); it != rctx->listeners.end(); ) {
    if (it->node == node && it->event_type == event_type && !it->is_handler_property) {
      JSValue* stored_cb = static_cast<JSValue*>(it->callback);
      if (JS_VALUE_GET_TAG(*stored_cb) == JS_VALUE_GET_TAG(argv[1]) &&
          JS_VALUE_GET_PTR(*stored_cb) == JS_VALUE_GET_PTR(argv[1])) {
        JS_FreeValue(ctx, *stored_cb);
        delete stored_cb;
        it = rctx->listeners.erase(it);
        break; // remove first matching listener only (DOM spec)
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
  return JS_UNDEFINED;
}

// ── dispatchEvent (shared target+bubbling logic) ──────────────────────────────

// Dispatches `event_obj` at `target_node`, walking up ancestors when
// event.bubbles is true. Honors stopPropagation()/stopImmediatePropagation()
// set by listeners via the Event prototype methods above. Returns JS_FALSE
// when a cancelable event had preventDefault() called, JS_TRUE otherwise
// (matching EventTarget.dispatchEvent's return value), or JS_EXCEPTION if a
// listener threw.
JSValue dispatch_event_on_target(JSContext* ctx, RuntimeContext* rctx, JSValue event_obj, lxb_dom_node_t* target_node) {
  JSValue type_val = JS_GetPropertyStr(ctx, event_obj, "type");
  const char* type_str = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);
  if (!type_str) return JS_TRUE;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  bool bubbles = get_bool_prop(ctx, event_obj, "bubbles");
  bool composed = get_bool_prop(ctx, event_obj, "composed");

  lxb_dom_node_t* doc_node = nullptr;
  if (rctx->document) {
    doc_node = lxb_dom_interface_node(lxb_dom_interface_document(
        static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));
  }

  JS_SetPropertyStr(ctx, event_obj, "target", make_element(ctx, target_node));
  JS_SetPropertyStr(ctx, event_obj, "__propagationStopped", JS_NewBool(ctx, false));

  std::vector<lxb_dom_node_t*> chain;
  chain.push_back(target_node);
  if (bubbles) {
    // Climb ancestors; when we run off the top of a shadow tree (parent ==
    // nullptr) and the event is composed, retarget through the shadow host
    // (found by reverse-lookup in element_shadow_roots) and keep climbing
    // into the light tree above it — this is how a composed event (e.g.
    // 'click') escapes a shadow boundary while a non-composed one stays
    // contained. Simplification: this sandbox has no separate capture phase,
    // so composedPath()/retargeting are only computed along the bubble chain
    // — a non-bubbling event's composedPath() is just [target].
    lxb_dom_node_t* boundary = target_node;
    lxb_dom_node_t* p = target_node->parent;
    while (true) {
      if (p) {
        chain.push_back(p);
        boundary = p;
        p = p->parent;
        continue;
      }
      if (!composed) break;
      void* host = nullptr;
      for (auto& kv : rctx->element_shadow_roots) {
        if (kv.second == static_cast<void*>(boundary)) { host = kv.first; break; }
      }
      if (!host) break;
      lxb_dom_node_t* host_node = static_cast<lxb_dom_node_t*>(host);
      chain.push_back(host_node);
      boundary = host_node;
      p = host_node->parent;
    }
  }

  {
    JSValue path_arr = JS_NewArray(ctx);
    for (size_t i = 0; i < chain.size(); i++) {
      lxb_dom_node_t* level = chain[i];
      JSValue entry = (level == doc_node)
          ? ([&]() { JSValue g = JS_GetGlobalObject(ctx); JSValue d = JS_GetPropertyStr(ctx, g, "document"); JS_FreeValue(ctx, g); return d; })()
          : make_element(ctx, level);
      JS_SetPropertyUint32(ctx, path_arr, (uint32_t)i, entry);
    }
    JS_SetPropertyStr(ctx, event_obj, "__composedPath", path_arr);
  }

  for (lxb_dom_node_t* level : chain) {
    if (get_bool_prop(ctx, event_obj, "__propagationStopped")) break;

    JSValue current_target;
    if (level == doc_node) {
      JSValue global = JS_GetGlobalObject(ctx);
      current_target = JS_GetPropertyStr(ctx, global, "document");
      JS_FreeValue(ctx, global);
    } else {
      current_target = make_element(ctx, level);
    }

    JS_SetPropertyStr(ctx, event_obj, "currentTarget", JS_DupValue(ctx, current_target));
    JS_SetPropertyStr(ctx, event_obj, "__immediatePropagationStopped", JS_NewBool(ctx, false));

    std::vector<JSValue> cbs_to_fire;
    std::vector<JSValue*> cb_ptrs;
    std::vector<bool> cb_is_handler_property;
    for (auto& listener : rctx->listeners) {
      if (listener.node == level && listener.event_type == event_type) {
        JSValue* cb = static_cast<JSValue*>(listener.callback);
        cbs_to_fire.push_back(JS_DupValue(ctx, *cb));
        cb_ptrs.push_back(cb);
        cb_is_handler_property.push_back(listener.is_handler_property);
      }
    }

    for (size_t i = 0; i < cbs_to_fire.size(); i++) {
      if (get_bool_prop(ctx, event_obj, "__immediatePropagationStopped")) {
        JS_FreeValue(ctx, cbs_to_fire[i]);
        continue;
      }
      bool still_registered = false;
      for (auto& l : rctx->listeners) {
        if (l.node == level && l.event_type == event_type) {
          JSValue* stored = static_cast<JSValue*>(l.callback);
          if (JS_VALUE_GET_PTR(*stored) == JS_VALUE_GET_PTR(cbs_to_fire[i])) {
            still_registered = true;
            break;
          }
        }
      }
      if (!still_registered) {
        JS_FreeValue(ctx, cbs_to_fire[i]);
        continue;
      }
      JSValue ret;
      if (cb_is_handler_property[i] && event_type == "error") {
        JSValue message_val = JS_GetPropertyStr(ctx, event_obj, "message");
        JSValue error_val = JS_GetPropertyStr(ctx, event_obj, "error");
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue location_val = JS_GetPropertyStr(ctx, global, "location");
        JSValue source_val = JS_GetPropertyStr(ctx, location_val, "href");
        JS_FreeValue(ctx, location_val);
        JS_FreeValue(ctx, global);
        JSValue lineno_val = JS_NewInt32(ctx, 0);
        JSValue colno_val = JS_NewInt32(ctx, 0);
        JSValue args[5] = { message_val, source_val, lineno_val, colno_val, error_val };
        ret = JS_Call(ctx, cbs_to_fire[i], current_target, 5, args);
        JS_FreeValue(ctx, message_val);
        JS_FreeValue(ctx, source_val);
        JS_FreeValue(ctx, lineno_val);
        JS_FreeValue(ctx, colno_val);
        JS_FreeValue(ctx, error_val);
      } else {
        ret = JS_Call(ctx, cbs_to_fire[i], current_target, 1, &event_obj);
      }
      JS_FreeValue(ctx, cbs_to_fire[i]);
      if (JS_IsException(ret)) {
        JS_FreeValue(ctx, ret);
        for (size_t j = i + 1; j < cbs_to_fire.size(); j++) JS_FreeValue(ctx, cbs_to_fire[j]);
        JS_FreeValue(ctx, current_target);

        JSValue ex = JS_GetException(ctx);
        std::string err = "Event callback error";
        JSValue msg_prop = JS_GetPropertyStr(ctx, ex, "message");
        if (!JS_IsException(msg_prop) && !JS_IsUndefined(msg_prop)) {
          const char* s = JS_ToCString(ctx, msg_prop);
          if (s) { err = s; JS_FreeCString(ctx, s); }
        }
        JS_FreeValue(ctx, msg_prop);
        JS_FreeValue(ctx, ex);
        JS_ThrowInternalError(ctx, "%s", err.c_str());
        return JS_EXCEPTION;
      }
      if (cb_is_handler_property[i] && JS_IsBool(ret) && JS_ToBool(ctx, ret) == 0 &&
          get_bool_prop(ctx, event_obj, "cancelable")) {
        JS_SetPropertyStr(ctx, event_obj, "defaultPrevented", JS_NewBool(ctx, true));
      }
      JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, current_target);
  }

  bool defaultPrevented = get_bool_prop(ctx, event_obj, "defaultPrevented");
  return JS_NewBool(ctx, !defaultPrevented);
}

JSValue js_el_dispatchEvent(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  auto* node = unwrap_node(ctx, this_val);
  if (!node) return JS_TRUE;
  return dispatch_event_on_target(ctx, rctx, argv[0], node);
}

// ── document.addEventListener / dispatchEvent ─────────────────────────────────

JSValue js_doc_addEventListener(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  if (!rctx->document) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  void* doc_node = lxb_dom_interface_node(
      lxb_dom_interface_document(
          static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));

  for (auto& l : rctx->listeners) {
    if (l.node == doc_node && l.event_type == event_type && !l.is_handler_property) {
      JSValue* stored = static_cast<JSValue*>(l.callback);
      if (JS_VALUE_GET_TAG(*stored) == JS_VALUE_GET_TAG(argv[1]) &&
          JS_VALUE_GET_PTR(*stored) == JS_VALUE_GET_PTR(argv[1]))
        return JS_UNDEFINED;
    }
  }

  EventListener listener;
  listener.node = doc_node;
  listener.event_type = event_type;
  listener.callback = new JSValue(JS_DupValue(ctx, argv[1]));
  rctx->listeners.push_back(std::move(listener));
  return JS_UNDEFINED;
}

JSValue js_doc_removeEventListener(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 2 || !JS_IsFunction(ctx, argv[1])) return JS_UNDEFINED;
  if (!rctx->document) return JS_UNDEFINED;

  const char* type_str = JS_ToCString(ctx, argv[0]);
  if (!type_str) return JS_UNDEFINED;
  std::string event_type(type_str);
  JS_FreeCString(ctx, type_str);

  void* doc_node = lxb_dom_interface_node(
      lxb_dom_interface_document(
          static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));

  for (auto it = rctx->listeners.begin(); it != rctx->listeners.end(); ) {
    if (it->node == doc_node && it->event_type == event_type) {
      JSValue* stored_cb = static_cast<JSValue*>(it->callback);
      if (JS_VALUE_GET_TAG(*stored_cb) == JS_VALUE_GET_TAG(argv[1]) &&
          JS_VALUE_GET_PTR(*stored_cb) == JS_VALUE_GET_PTR(argv[1])) {
        JS_FreeValue(ctx, *stored_cb);
        delete stored_cb;
        rctx->listeners.erase(it);
        break; // remove first matching listener only (DOM spec)
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
  return JS_UNDEFINED;
}

JSValue js_doc_dispatchEvent(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  if (!rctx->document) return JS_TRUE;

  lxb_dom_node_t* node = lxb_dom_interface_node(
      lxb_dom_interface_document(
          static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));
  return dispatch_event_on_target(ctx, rctx, argv[0], node);
}

const char* kHandlerEventTypes[] = { "click", "load", "error", "unhandledrejection" };

JSValue get_handler_prop_for_node(JSContext* ctx, RuntimeContext* rctx, void* node, const std::string& event_type) {
  if (!rctx || !node) return JS_NULL;
  for (auto& l : rctx->listeners) {
    if (l.node == node && l.event_type == event_type && l.is_handler_property)
      return JS_DupValue(ctx, *static_cast<JSValue*>(l.callback));
  }
  return JS_NULL;
}

JSValue set_handler_prop_for_node(JSContext* ctx, RuntimeContext* rctx, void* node, const std::string& event_type, JSValue val) {
  if (!rctx || !node) return JS_UNDEFINED;
  for (auto it = rctx->listeners.begin(); it != rctx->listeners.end(); ++it) {
    if (it->node == node && it->event_type == event_type && it->is_handler_property) {
      JSValue* stored = static_cast<JSValue*>(it->callback);
      JS_FreeValue(ctx, *stored);
      delete stored;
      rctx->listeners.erase(it);
      break;
    }
  }
  if (JS_IsFunction(ctx, val)) {
    EventListener listener;
    listener.node = node;
    listener.event_type = event_type;
    listener.callback = new JSValue(JS_DupValue(ctx, val));
    listener.is_handler_property = true;
    rctx->listeners.push_back(std::move(listener));
  }
  return JS_UNDEFINED;
}

JSValue js_el_get_handler(JSContext* ctx, JSValue this_val, int magic) {
  auto* rctx = get_ctx(ctx);
  auto* node = unwrap_node(ctx, this_val);
  return get_handler_prop_for_node(ctx, rctx, node, kHandlerEventTypes[magic]);
}

JSValue js_el_set_handler(JSContext* ctx, JSValue this_val, JSValue val, int magic) {
  auto* rctx = get_ctx(ctx);
  auto* node = unwrap_node(ctx, this_val);
  return set_handler_prop_for_node(ctx, rctx, node, kHandlerEventTypes[magic], val);
}

void* doc_node_of(RuntimeContext* rctx) {
  if (!rctx || !rctx->document) return nullptr;
  return lxb_dom_interface_node(lxb_dom_interface_document(
      static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));
}

JSValue js_doc_get_handler(JSContext* ctx, JSValue, int magic) {
  auto* rctx = get_ctx(ctx);
  return get_handler_prop_for_node(ctx, rctx, doc_node_of(rctx), kHandlerEventTypes[magic]);
}

JSValue js_doc_set_handler(JSContext* ctx, JSValue, JSValue val, int magic) {
  auto* rctx = get_ctx(ctx);
  return set_handler_prop_for_node(ctx, rctx, doc_node_of(rctx), kHandlerEventTypes[magic], val);
}

void define_element_handler(JSContext* ctx, JSValue obj, const char* name, int magic) {
  JSAtom atom = JS_NewAtom(ctx, name);
  JSValue get_fn = JS_NewCFunction2(ctx, (JSCFunction*)js_el_get_handler, name, 0, JS_CFUNC_getter_magic, magic);
  JSValue set_fn = JS_NewCFunction2(ctx, (JSCFunction*)js_el_set_handler, name, 1, JS_CFUNC_setter_magic, magic);
  JS_DefinePropertyGetSet(ctx, obj, atom, get_fn, set_fn, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, atom);
}

void define_doc_handler(JSContext* ctx, JSValue obj, const char* name, int magic) {
  JSAtom atom = JS_NewAtom(ctx, name);
  JSValue get_fn = JS_NewCFunction2(ctx, (JSCFunction*)js_doc_get_handler, name, 0, JS_CFUNC_getter_magic, magic);
  JSValue set_fn = JS_NewCFunction2(ctx, (JSCFunction*)js_doc_set_handler, name, 1, JS_CFUNC_setter_magic, magic);
  JS_DefinePropertyGetSet(ctx, obj, atom, get_fn, set_fn, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, atom);
}

JSValue make_simple_event(JSContext* ctx, const char* type, bool bubbles, bool cancelable) {
  JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
  JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type));
  JS_SetPropertyStr(ctx, obj, "bubbles", JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable", JS_NewBool(ctx, cancelable));
  JS_SetPropertyStr(ctx, obj, "defaultPrevented", JS_NewBool(ctx, false));
  return obj;
}

void fire_simple_event(JSContext* ctx, RuntimeContext* rctx, const char* type, bool bubbles, bool cancelable, lxb_dom_node_t* node) {
  JSValue evt = make_simple_event(ctx, type, bubbles, cancelable);
  JSValue r = dispatch_event_on_target(ctx, rctx, evt, node);
  JS_FreeValue(ctx, evt);
  if (JS_IsException(r)) JS_FreeValue(ctx, JS_GetException(ctx));
  else JS_FreeValue(ctx, r);
}

JSValue js_el_click(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* rctx = get_ctx(ctx);
  auto* node = unwrap_node(ctx, this_val);
  if (!rctx || !node) return JS_UNDEFINED;
  fire_simple_event(ctx, rctx, "click", true, true, node);
  return JS_UNDEFINED;
}

JSValue js_el_focus(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* rctx = get_ctx(ctx);
  auto* node = unwrap_node(ctx, this_val);
  if (!rctx || !node || rctx->active_element == node) return JS_UNDEFINED;

  void* previous = rctx->active_element;
  rctx->active_element = node;
  if (previous) fire_simple_event(ctx, rctx, "blur", false, false, static_cast<lxb_dom_node_t*>(previous));
  fire_simple_event(ctx, rctx, "focus", false, false, node);
  return JS_UNDEFINED;
}

JSValue js_el_blur(JSContext* ctx, JSValue this_val, int, JSValue*) {
  auto* rctx = get_ctx(ctx);
  auto* node = unwrap_node(ctx, this_val);
  if (!rctx || !node || rctx->active_element != node) return JS_UNDEFINED;

  rctx->active_element = nullptr;
  fire_simple_event(ctx, rctx, "blur", false, false, node);
  return JS_UNDEFINED;
}

} // namespace

void EventBindings::install(JSContext* ctx) {
  if (js_event_class_id == 0) JS_NewClassID(&js_event_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_event_class_id, &js_event_class);

  // ── Event prototype ────────────────────────────────────────────────────────
  JSValue event_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, event_proto, "preventDefault",           JS_NewCFunction(ctx, js_event_preventDefault,           "preventDefault",           0));
  JS_SetPropertyStr(ctx, event_proto, "stopPropagation",          JS_NewCFunction(ctx, js_event_stopPropagation,          "stopPropagation",          0));
  JS_SetPropertyStr(ctx, event_proto, "stopImmediatePropagation", JS_NewCFunction(ctx, js_event_stopImmediatePropagation, "stopImmediatePropagation", 0));
  JS_SetPropertyStr(ctx, event_proto, "composedPath",             JS_NewCFunction(ctx, js_event_composedPath,             "composedPath",             0));
  JS_SetClassProto(ctx, js_event_class_id, event_proto);

  // ── Event / CustomEvent constructors ───────────────────────────────────────
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "Event",
      JS_NewCFunction2(ctx, js_Event_constructor, "Event", 1, JS_CFUNC_constructor, 0));
  JS_SetPropertyStr(ctx, global, "CustomEvent",
      JS_NewCFunction2(ctx, js_CustomEvent_constructor, "CustomEvent", 2, JS_CFUNC_constructor, 0));
  JS_SetPropertyStr(ctx, global, "KeyboardEvent",
      JS_NewCFunction2(ctx, js_KeyboardEvent_constructor, "KeyboardEvent", 2, JS_CFUNC_constructor, 0));
  JS_SetPropertyStr(ctx, global, "MouseEvent",
      JS_NewCFunction2(ctx, js_MouseEvent_constructor, "MouseEvent", 2, JS_CFUNC_constructor, 0));

  // ── addEventListener/removeEventListener/dispatchEvent on Node + Element ──
  // Added to the Node proto so text/comment nodes can also receive events.
  JSValue node_proto = JS_GetClassProto(ctx, js_node_class_id);
  JS_SetPropertyStr(ctx, node_proto, "addEventListener",    JS_NewCFunction(ctx, js_el_addEventListener,    "addEventListener",    2));
  JS_SetPropertyStr(ctx, node_proto, "removeEventListener", JS_NewCFunction(ctx, js_el_removeEventListener, "removeEventListener", 2));
  JS_SetPropertyStr(ctx, node_proto, "dispatchEvent",       JS_NewCFunction(ctx, js_el_dispatchEvent,       "dispatchEvent",       1));

  define_element_handler(ctx, node_proto, "onclick", 0);
  define_element_handler(ctx, node_proto, "onload",  1);
  define_element_handler(ctx, node_proto, "onerror", 2);
  JS_FreeValue(ctx, node_proto);

  JSValue element_proto = JS_GetClassProto(ctx, js_element_class_id);
  JS_SetPropertyStr(ctx, element_proto, "click", JS_NewCFunction(ctx, js_el_click, "click", 0));
  JS_SetPropertyStr(ctx, element_proto, "focus", JS_NewCFunction(ctx, js_el_focus, "focus", 0));
  JS_SetPropertyStr(ctx, element_proto, "blur",  JS_NewCFunction(ctx, js_el_blur,  "blur",  0));
  JS_FreeValue(ctx, element_proto);

  // ── addEventListener/removeEventListener/dispatchEvent on document ────────
  // DocumentBindings::install() must have already run so globalThis.document exists.
  JSValue doc = JS_GetPropertyStr(ctx, global, "document");
  JS_SetPropertyStr(ctx, doc, "addEventListener",    JS_NewCFunction(ctx, js_doc_addEventListener,    "addEventListener",    2));
  JS_SetPropertyStr(ctx, doc, "removeEventListener", JS_NewCFunction(ctx, js_doc_removeEventListener, "removeEventListener", 2));
  JS_SetPropertyStr(ctx, doc, "dispatchEvent",       JS_NewCFunction(ctx, js_doc_dispatchEvent,       "dispatchEvent",       1));

  define_doc_handler(ctx, doc, "onclick", 0);
  define_doc_handler(ctx, doc, "onload",  1);
  define_doc_handler(ctx, doc, "onerror", 2);
  JS_FreeValue(ctx, doc);

  // ── addEventListener/removeEventListener/dispatchEvent on window ──────────
  // window === globalThis (see QuickJSRuntime::initialize()) and has no native
  // node of its own to key listeners by, so js_doc_addEventListener/
  // js_doc_removeEventListener/js_doc_dispatchEvent (which never read `this`
  // — they always resolve the target through rctx->document) are reused
  // verbatim. This means window.addEventListener(...) and
  // document.addEventListener(...) share one listener list/target in this
  // sandbox — a deliberate simplification, not a real separate Window
  // EventTarget.
  JS_SetPropertyStr(ctx, global, "addEventListener",    JS_NewCFunction(ctx, js_doc_addEventListener,    "addEventListener",    2));
  JS_SetPropertyStr(ctx, global, "removeEventListener", JS_NewCFunction(ctx, js_doc_removeEventListener, "removeEventListener", 2));
  JS_SetPropertyStr(ctx, global, "dispatchEvent",       JS_NewCFunction(ctx, js_doc_dispatchEvent,       "dispatchEvent",       1));

  define_doc_handler(ctx, global, "onclick", 0);
  define_doc_handler(ctx, global, "onload",  1);
  define_doc_handler(ctx, global, "onerror", 2);
  define_doc_handler(ctx, global, "onunhandledrejection", 3);

  JS_FreeValue(ctx, global);
}

void EventBindings::dispatchErrorEvent(JSContext* ctx, const std::string& message, JSValue error_value) {
  auto* rctx = get_ctx(ctx);
  void* node = doc_node_of(rctx);
  if (!rctx || !node) return;

  JSValue evt = make_simple_event(ctx, "error", false, true);
  JS_SetPropertyStr(ctx, evt, "message", JS_NewStringLen(ctx, message.data(), message.size()));
  JS_SetPropertyStr(ctx, evt, "error", JS_DupValue(ctx, error_value));
  JSValue r = dispatch_event_on_target(ctx, rctx, evt, static_cast<lxb_dom_node_t*>(node));
  JS_FreeValue(ctx, evt);
  if (JS_IsException(r)) JS_FreeValue(ctx, JS_GetException(ctx));
  else JS_FreeValue(ctx, r);
}

void EventBindings::dispatchUnhandledRejectionEvent(JSContext* ctx, JSValue reason) {
  auto* rctx = get_ctx(ctx);
  void* node = doc_node_of(rctx);
  if (!rctx || !node) return;

  JSValue evt = make_simple_event(ctx, "unhandledrejection", false, true);
  JS_SetPropertyStr(ctx, evt, "reason", JS_DupValue(ctx, reason));
  JSValue r = dispatch_event_on_target(ctx, rctx, evt, static_cast<lxb_dom_node_t*>(node));
  JS_FreeValue(ctx, evt);
  if (JS_IsException(r)) JS_FreeValue(ctx, JS_GetException(ctx));
  else JS_FreeValue(ctx, r);
}

} // namespace margelo::nitro::nitrojsdom
