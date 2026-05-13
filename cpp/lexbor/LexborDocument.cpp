#include "LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/dom/dom.h>
#include <stdexcept>

namespace margelo::nitro::nitrojsdom {

namespace {

static std::string serializeNode(lxb_dom_node_t* node) {
  lexbor_str_t str = {0};
  lxb_html_serialize_tree_str(node, &str);
  std::string result(reinterpret_cast<char*>(str.data), str.length);
  lexbor_str_destroy(&str, NULL, false);
  return result;
}

struct FindFirstCtx { lxb_dom_element_t* result = nullptr; };

static lxb_status_t findFirstCallback(lxb_dom_node_t* node, lxb_css_selector_specificity_t*, void* ctx) {
  auto* c = static_cast<FindFirstCtx*>(ctx);
  c->result = lxb_dom_interface_element(node);
  return LXB_STATUS_STOP;
}

static lxb_status_t findAllCallback(lxb_dom_node_t* node, lxb_css_selector_specificity_t*, void* ctx) {
  auto* vec = static_cast<std::vector<std::string>*>(ctx);
  vec->push_back(serializeNode(node));
  return LXB_STATUS_OK;
}

} // namespace

LexborDocument::LexborDocument() {
  _cssParser = lxb_css_parser_create();
  lxb_css_parser_init(_cssParser, nullptr);
  _selectors = lxb_selectors_create();
  lxb_selectors_init(_selectors);
}

LexborDocument::~LexborDocument() {
  if (_selectors) lxb_selectors_destroy(_selectors, true);
  if (_cssParser) lxb_css_parser_destroy(_cssParser, true);
  if (_document) {
    lxb_html_document_destroy(_document);
    _document = nullptr;
  }
}

lxb_dom_element_t* LexborDocument::findFirst(const std::string& selector) const {
  if (!_document) return nullptr;
  lxb_css_selector_list_t* list = lxb_css_selectors_parse(_cssParser,
      reinterpret_cast<const lxb_char_t*>(selector.data()), selector.size());
  FindFirstCtx ctx;
  if (list) {
    lxb_selectors_find(_selectors, lxb_dom_interface_node(_document), list, findFirstCallback, &ctx);
    lxb_css_selector_list_destroy_memory(list);
  }
  return ctx.result;
}

void LexborDocument::parse(const std::string& html) {
  if (_document) lxb_html_document_destroy(_document);
  _document = lxb_html_document_create();
  if (!_document) throw std::runtime_error("Lexbor: failed to create document");
  lxb_status_t status = lxb_html_document_parse(_document,
      reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
  if (status != LXB_STATUS_OK) {
    lxb_html_document_destroy(_document);
    _document = nullptr;
    throw std::runtime_error("Lexbor: failed to parse HTML");
  }
}

std::string LexborDocument::serialize() const {
  if (!_document) return "";
  return serializeNode(lxb_dom_interface_node(_document));
}

std::optional<std::string> LexborDocument::querySelector(const std::string& selector) const {
  auto* el = findFirst(selector);
  if (!el) return std::nullopt;
  return serializeNode(lxb_dom_interface_node(el));
}

std::vector<std::string> LexborDocument::querySelectorAll(const std::string& selector) const {
  if (!_document) return {};
  lxb_css_selector_list_t* list = lxb_css_selectors_parse(_cssParser,
      reinterpret_cast<const lxb_char_t*>(selector.data()), selector.size());
  std::vector<std::string> results;
  if (list) {
    lxb_selectors_find(_selectors, lxb_dom_interface_node(_document), list, findAllCallback, &results);
    lxb_css_selector_list_destroy_memory(list);
  }
  return results;
}

std::optional<std::string> LexborDocument::getAttribute(const std::string& selector, const std::string& attr) const {
  auto* el = findFirst(selector);
  if (!el) return std::nullopt;
  size_t val_len;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>(attr.data()), attr.size(), &val_len);
  if (!val) return std::nullopt;
  return std::string(reinterpret_cast<const char*>(val), val_len);
}

void LexborDocument::setAttribute(const std::string& selector, const std::string& attr, const std::string& value) {
  auto* el = findFirst(selector);
  if (!el) return;
  lxb_dom_element_set_attribute(el,
      reinterpret_cast<const lxb_char_t*>(attr.data()), attr.size(),
      reinterpret_cast<const lxb_char_t*>(value.data()), value.size());
}

std::optional<std::string> LexborDocument::getTextContent(const std::string& selector) const {
  auto* el = findFirst(selector);
  if (!el) return std::nullopt;
  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  size_t len;
  lxb_char_t* text = lxb_dom_node_text_content(node, &len);
  if (!text) return std::nullopt;
  std::string result(reinterpret_cast<char*>(text), len);
  lxb_dom_document_destroy_text(node->owner_document, text);
  return result;
}

std::optional<std::string> LexborDocument::getInnerHTML(const std::string& selector) const {
  auto* el = findFirst(selector);
  if (!el) return std::nullopt;
  std::string result;
  lxb_dom_node_t* child = lxb_dom_interface_node(el)->first_child;
  while (child) {
    result += serializeNode(child);
    child = child->next;
  }
  return result;
}

void LexborDocument::setInnerHTML(const std::string& selector, const std::string& html) {
  auto* el = findFirst(selector);
  if (!el) return;
  lxb_dom_node_t* node = lxb_dom_interface_node(el);
  // Remove existing children
  lxb_dom_node_t* child = node->first_child;
  while (child) {
    lxb_dom_node_t* next = child->next;
    lxb_dom_node_remove(child);
    lxb_dom_node_destroy_deep(child);
    child = next;
  }
  // Parse and insert new fragment
  lxb_html_document_parse_fragment(_document, el,
      reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
}

} // namespace margelo::nitro::nitrojsdom
