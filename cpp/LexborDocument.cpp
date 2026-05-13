#include "LexborDocument.hpp"

// TODO: include Lexbor headers once third_party/lexbor is added:
// #include <lexbor/html/html.h>
// #include <lexbor/css/css.h>
// #include <lexbor/selectors/selectors.h>

namespace margelo::nitro::nitrojsdom {

LexborDocument::LexborDocument() = default;

LexborDocument::~LexborDocument() {
  // TODO: lxb_html_document_destroy((lxb_html_document_t*)_document);
  _document = nullptr;
}

void LexborDocument::parse(const std::string& html) {
  _rawHtml = html;

  // TODO: Real Lexbor parsing:
  // lxb_html_document_t* doc = lxb_html_document_create();
  // lxb_status_t status = lxb_html_document_parse(doc,
  //     (const lxb_char_t*)html.c_str(), html.size());
  // _document = doc;
}

std::string LexborDocument::serialize() const {
  // TODO: lxb_html_serialize_str(...) on the document root
  return _rawHtml;
}

std::optional<std::string> LexborDocument::querySelector(const std::string& /*selector*/) const {
  // TODO: Use lxb_selectors_t to find the first matching element, then serialize it.
  return std::nullopt;
}

std::vector<std::string> LexborDocument::querySelectorAll(const std::string& /*selector*/) const {
  // TODO: Use lxb_selectors_t to find all matching elements, serialize each.
  return {};
}

std::optional<std::string> LexborDocument::getAttribute(const std::string& /*selector*/, const std::string& /*attr*/) const {
  // TODO: Find element via querySelector, then lxb_dom_element_get_attribute(...).
  return std::nullopt;
}

void LexborDocument::setAttribute(const std::string& /*selector*/, const std::string& /*attr*/, const std::string& /*value*/) {
  // TODO: Find element via querySelector, then lxb_dom_element_set_attribute(...).
}

std::optional<std::string> LexborDocument::getTextContent(const std::string& /*selector*/) const {
  // TODO: Find element, then collect lxb_dom_node_text_content(...).
  return std::nullopt;
}

std::optional<std::string> LexborDocument::getInnerHTML(const std::string& /*selector*/) const {
  // TODO: Find element, serialize its children via lxb_html_serialize_tree_str(...).
  return std::nullopt;
}

void LexborDocument::setInnerHTML(const std::string& /*selector*/, const std::string& /*html*/) {
  // TODO: Find element, replace children by re-parsing the html fragment.
}

} // namespace margelo::nitro::nitrojsdom
