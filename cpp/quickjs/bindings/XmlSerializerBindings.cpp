#include "XmlSerializerBindings.hpp"
#include "../DOMBindingsInternal.hpp"
#include <string>

namespace margelo::nitro::nitrojsdom {

namespace {

JSClassID js_xml_serializer_class_id = 0;
JSClassDef js_xml_serializer_class = { "XMLSerializer", .finalizer = nullptr };

JSValue js_xmlserializer_constructor(JSContext* ctx, JSValue, int, JSValue*) {
  return JS_NewObjectClass(ctx, js_xml_serializer_class_id);
}

JSValue js_xmlserializer_serializeToString(JSContext* ctx, JSValue, int argc, JSValue* argv) {
  if (argc < 1) return JS_NewString(ctx, "");
  auto* node = unwrap_node(ctx, argv[0]);
  if (!node) return JS_NewString(ctx, "");
  std::string result = serialize_node(node);
  return JS_NewStringLen(ctx, result.data(), result.size());
}

} // namespace

void XmlSerializerBindings::install(JSContext* ctx) {
  if (js_xml_serializer_class_id == 0) JS_NewClassID(&js_xml_serializer_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_xml_serializer_class_id, &js_xml_serializer_class);

  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, proto, "serializeToString",
      JS_NewCFunction(ctx, js_xmlserializer_serializeToString, "serializeToString", 1));
  JS_SetClassProto(ctx, js_xml_serializer_class_id, JS_DupValue(ctx, proto));

  JSValue ctor = JS_NewCFunction2(ctx, js_xmlserializer_constructor, "XMLSerializer", 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor(ctx, ctor, proto);
  JS_FreeValue(ctx, proto);

  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "XMLSerializer", ctor);
  JS_FreeValue(ctx, global);
}

} // namespace margelo::nitro::nitrojsdom
