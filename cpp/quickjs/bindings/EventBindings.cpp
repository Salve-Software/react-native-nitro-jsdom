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

  JS_SetPropertyStr(ctx, obj, "defaultPrevented",  JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "bubbles",           JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable",        JS_NewBool(ctx, cancelable));
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

  JS_SetPropertyStr(ctx, obj, "detail",           detail);
  JS_SetPropertyStr(ctx, obj, "bubbles",          JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, obj, "cancelable",       JS_NewBool(ctx, cancelable));
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
    if (l.node == node && l.event_type == event_type) {
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
    if (it->node == node && it->event_type == event_type) {
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
    for (lxb_dom_node_t* p = target_node->parent; p; p = p->parent) chain.push_back(p);
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
    for (auto& listener : rctx->listeners) {
      if (listener.node == level && listener.event_type == event_type) {
        JSValue* cb = static_cast<JSValue*>(listener.callback);
        cbs_to_fire.push_back(JS_DupValue(ctx, *cb));
        cb_ptrs.push_back(cb);
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
      JSValue ret = JS_Call(ctx, cbs_to_fire[i], current_target, 1, &event_obj);
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
    if (l.node == doc_node && l.event_type == event_type) {
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

JSValue js_doc_dispatchEvent(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  auto* rctx = get_ctx(ctx);
  if (!rctx || argc < 1) return JS_TRUE;
  if (!rctx->document) return JS_TRUE;

  lxb_dom_node_t* node = lxb_dom_interface_node(
      lxb_dom_interface_document(
          static_cast<lxb_html_document_t*>(rctx->document->documentHtmlPtr())));
  return dispatch_event_on_target(ctx, rctx, argv[0], node);
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
  JS_FreeValue(ctx, node_proto);

  // ── addEventListener/dispatchEvent on document ─────────────────────────────
  // DocumentBindings::install() must have already run so globalThis.document exists.
  JSValue doc = JS_GetPropertyStr(ctx, global, "document");
  JS_SetPropertyStr(ctx, doc, "addEventListener", JS_NewCFunction(ctx, js_doc_addEventListener, "addEventListener", 2));
  JS_SetPropertyStr(ctx, doc, "dispatchEvent",    JS_NewCFunction(ctx, js_doc_dispatchEvent,    "dispatchEvent",    1));
  JS_FreeValue(ctx, doc);

  // ── addEventListener/dispatchEvent on window ───────────────────────────────
  // window === globalThis (see QuickJSRuntime::initialize()) and has no native
  // node of its own to key listeners by, so js_doc_addEventListener/
  // js_doc_dispatchEvent (which never read `this` — they always resolve the
  // target through rctx->document) are reused verbatim. This means
  // window.addEventListener(...) and document.addEventListener(...) share one
  // listener list/target in this sandbox — a deliberate simplification, not a
  // real separate Window EventTarget.
  JS_SetPropertyStr(ctx, global, "addEventListener", JS_NewCFunction(ctx, js_doc_addEventListener, "addEventListener", 2));
  JS_SetPropertyStr(ctx, global, "dispatchEvent",    JS_NewCFunction(ctx, js_doc_dispatchEvent,    "dispatchEvent",    1));

  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
