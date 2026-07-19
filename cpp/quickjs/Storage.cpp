#include "Storage.hpp"
#include "quickjs.h"

namespace margelo::nitro::nitrojsdom {

// ── Storage ───────────────────────────────────────────────────────────────────

std::optional<std::string> Storage::getItem(const std::string& key) const {
  auto it = _data.find(key);
  if (it == _data.end()) return std::nullopt;
  return it->second;
}

void Storage::setItem(const std::string& key, const std::string& value) {
  auto it = _data.find(key);
  if (it == _data.end()) {
    _order.push_back(key);
  }
  _data[key] = value;
}

void Storage::removeItem(const std::string& key) {
  auto it = _data.find(key);
  if (it == _data.end()) return;
  _data.erase(it);
  for (auto orderIt = _order.begin(); orderIt != _order.end(); ++orderIt) {
    if (*orderIt == key) {
      _order.erase(orderIt);
      break;
    }
  }
}

void Storage::clear() {
  _data.clear();
  _order.clear();
}

std::optional<std::string> Storage::key(size_t index) const {
  if (index >= _order.size()) return std::nullopt;
  return _order[index];
}

// ── JS bindings ───────────────────────────────────────────────────────────────

static JSClassID js_storage_class_id = 0;
static JSClassDef js_storage_class = { "Storage", .finalizer = nullptr };

static Storage* unwrap_storage(JSValue this_val) {
  return static_cast<Storage*>(JS_GetOpaque(this_val, js_storage_class_id));
}

static std::string js_to_std_string(JSContext* ctx, JSValue v) {
  JSValue str_val = JS_ToString(ctx, v);
  std::string out;
  const char* s = JS_ToCString(ctx, str_val);
  if (s) { out = s; JS_FreeCString(ctx, s); }
  JS_FreeValue(ctx, str_val);
  return out;
}

static JSValue js_storage_getItem(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  Storage* storage = unwrap_storage(this_val);
  if (!storage || argc < 1) return JS_NULL;
  auto value = storage->getItem(js_to_std_string(ctx, argv[0]));
  if (!value.has_value()) return JS_NULL;
  return JS_NewStringLen(ctx, value->c_str(), value->size());
}

static JSValue js_storage_setItem(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  Storage* storage = unwrap_storage(this_val);
  if (!storage || argc < 2) return JS_UNDEFINED;
  storage->setItem(js_to_std_string(ctx, argv[0]), js_to_std_string(ctx, argv[1]));
  return JS_UNDEFINED;
}

static JSValue js_storage_removeItem(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  Storage* storage = unwrap_storage(this_val);
  if (!storage || argc < 1) return JS_UNDEFINED;
  storage->removeItem(js_to_std_string(ctx, argv[0]));
  return JS_UNDEFINED;
}

static JSValue js_storage_clear(JSContext* ctx, JSValue this_val, int, JSValue*) {
  Storage* storage = unwrap_storage(this_val);
  if (storage) storage->clear();
  return JS_UNDEFINED;
}

static JSValue js_storage_key(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  Storage* storage = unwrap_storage(this_val);
  if (!storage || argc < 1) return JS_NULL;
  int32_t index = 0;
  if (JS_ToInt32(ctx, &index, argv[0]) < 0 || index < 0) return JS_NULL;
  auto k = storage->key(static_cast<size_t>(index));
  if (!k.has_value()) return JS_NULL;
  return JS_NewStringLen(ctx, k->c_str(), k->size());
}

static JSValue js_storage_get_length(JSContext* ctx, JSValue this_val) {
  Storage* storage = unwrap_storage(this_val);
  return JS_NewInt32(ctx, storage ? static_cast<int32_t>(storage->length()) : 0);
}

void installStorage(JSContext* ctx, const char* globalName, Storage* storage) {
  if (js_storage_class_id == 0) {
    JS_NewClassID(&js_storage_class_id);
  }
  JS_NewClass(JS_GetRuntime(ctx), js_storage_class_id, &js_storage_class);

  JSValue obj = JS_NewObjectClass(ctx, js_storage_class_id);
  JS_SetOpaque(obj, storage);

  JS_SetPropertyStr(ctx, obj, "getItem", JS_NewCFunction(ctx, js_storage_getItem, "getItem", 1));
  JS_SetPropertyStr(ctx, obj, "setItem", JS_NewCFunction(ctx, js_storage_setItem, "setItem", 2));
  JS_SetPropertyStr(ctx, obj, "removeItem", JS_NewCFunction(ctx, js_storage_removeItem, "removeItem", 1));
  JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx, js_storage_clear, "clear", 0));
  JS_SetPropertyStr(ctx, obj, "key", JS_NewCFunction(ctx, js_storage_key, "key", 1));

  JSAtom atom = JS_NewAtom(ctx, "length");
  JSValue getter = JS_NewCFunction2(ctx, (JSCFunction*)js_storage_get_length, "length", 0, JS_CFUNC_getter, 0);
  JS_DefinePropertyGetSet(ctx, obj, atom, getter, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, atom);

  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, globalName, obj);
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
