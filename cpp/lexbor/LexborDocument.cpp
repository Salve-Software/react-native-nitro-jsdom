#include "LexborDocument.hpp"
#include <lexbor/html/html.h>
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>
#include <lexbor/dom/dom.h>
#include <stdexcept>

namespace margelo::nitro::nitrojsdom {

// ── Callbacks ───────────────────────────────────────────────────────────────

namespace {

static lxb_status_t serializeCallback(const lxb_char_t* data, size_t len, void* ctx) {
  static_cast<std::string*>(ctx)->append(reinterpret_cast<const char*>(data), len);
  return LXB_STATUS_OK;
}

static std::string serializeNode(lxb_dom_node_t* node) {
  std::string result;
  lxb_html_serialize_tree_cb(node, serializeCallback, &result);
  return result;
}

struct FindFirstCtx { lxb_dom_element_t* result = nullptr; };

static lxb_status_t findFirstCallback(lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ctx) {
  static_cast<FindFirstCtx*>(ctx)->result = lxb_dom_interface_element(node);
  return LXB_STATUS_STOP;
}

static lxb_status_t findAllStringCallback(lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ctx) {
  static_cast<std::vector<std::string>*>(ctx)->push_back(serializeNode(node));
  return LXB_STATUS_OK;
}

static lxb_status_t findAllPtrCallback(lxb_dom_node_t* node, lxb_css_selector_specificity_t, void* ctx) {
  static_cast<std::vector<void*>*>(ctx)->push_back(lxb_dom_interface_element(node));
  return LXB_STATUS_OK;
}

} // namespace

// ── Constructor / Destructor ─────────────────────────────────────────────────

LexborDocument::LexborDocument() {
  auto* parser = lxb_css_parser_create();
  lxb_css_parser_init(parser, nullptr);
  _cssParser = parser;

  auto* selectors = lxb_selectors_create();
  lxb_selectors_init(selectors);
  _selectors = selectors;
}

LexborDocument::~LexborDocument() {
  if (_selectors) lxb_selectors_destroy(static_cast<lxb_selectors_t*>(_selectors), true);
  if (_cssParser) lxb_css_parser_destroy(static_cast<lxb_css_parser_t*>(_cssParser), true);
  if (_document) {
    lxb_html_document_destroy(static_cast<lxb_html_document_t*>(_document));
    _document = nullptr;
  }
}

// ── Internal helpers ─────────────────────────────────────────────────────────

void* LexborDocument::findFirst(const std::string& selector) const {
  if (!_document) return nullptr;
  return findFirstFrom(lxb_dom_interface_node(static_cast<lxb_html_document_t*>(_document)), selector);
}

void* LexborDocument::findFirstFrom(void* startNode, const std::string& selector) const {
  if (!startNode) return nullptr;
  auto* parser    = static_cast<lxb_css_parser_t*>(_cssParser);
  auto* selectors = static_cast<lxb_selectors_t*>(_selectors);

  lxb_css_selector_list_t* list = lxb_css_selectors_parse(parser,
      reinterpret_cast<const lxb_char_t*>(selector.data()), selector.size());
  if (!list) return nullptr;

  FindFirstCtx ctx;
  lxb_selectors_find(selectors, static_cast<lxb_dom_node_t*>(startNode), list, findFirstCallback, &ctx);
  lxb_css_selector_list_destroy_memory(list);
  return ctx.result;
}

// ── Parse / Serialize ────────────────────────────────────────────────────────

void LexborDocument::parse(const std::string& html) {
  if (_document) lxb_html_document_destroy(static_cast<lxb_html_document_t*>(_document));
  auto* doc = lxb_html_document_create();
  if (!doc) throw std::runtime_error("Lexbor: failed to create document");
  lxb_status_t status = lxb_html_document_parse(doc,
      reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
  if (status != LXB_STATUS_OK) {
    lxb_html_document_destroy(doc);
    _document = nullptr;
    throw std::runtime_error("Lexbor: failed to parse HTML");
  }
  _document = doc;
}

std::string LexborDocument::serialize() const {
  if (!_document) return "";
  return serializeNode(lxb_dom_interface_node(static_cast<lxb_html_document_t*>(_document)));
}

// ── Element queries ──────────────────────────────────────────────────────────

void* LexborDocument::getElementById(const std::string& id) const {
  // [id="..."] is safe for any id value, including those with special CSS chars
  return findFirst("[id=\"" + id + "\"]");
}

void* LexborDocument::querySelector_el(const std::string& sel) const {
  return findFirst(sel);
}

std::vector<void*> LexborDocument::querySelectorAll_el(const std::string& sel) const {
  if (!_document) return {};
  auto* parser    = static_cast<lxb_css_parser_t*>(_cssParser);
  auto* selectors = static_cast<lxb_selectors_t*>(_selectors);
  auto* doc       = static_cast<lxb_html_document_t*>(_document);

  lxb_css_selector_list_t* list = lxb_css_selectors_parse(parser,
      reinterpret_cast<const lxb_char_t*>(sel.data()), sel.size());
  std::vector<void*> results;
  if (list) {
    lxb_selectors_find(selectors, lxb_dom_interface_node(doc), list, findAllPtrCallback, &results);
    lxb_css_selector_list_destroy_memory(list);
  }
  return results;
}

void* LexborDocument::querySelectorFromEl(void* element, const std::string& sel) const {
  if (!element) return nullptr;
  return findFirstFrom(lxb_dom_interface_node(static_cast<lxb_dom_element_t*>(element)), sel);
}

std::vector<void*> LexborDocument::querySelectorAllFromEl(void* element, const std::string& sel) const {
  if (!_document || !element) return {};
  auto* parser    = static_cast<lxb_css_parser_t*>(_cssParser);
  auto* selectors = static_cast<lxb_selectors_t*>(_selectors);
  auto* node      = lxb_dom_interface_node(static_cast<lxb_dom_element_t*>(element));

  lxb_css_selector_list_t* list = lxb_css_selectors_parse(parser,
      reinterpret_cast<const lxb_char_t*>(sel.data()), sel.size());
  std::vector<void*> results;
  if (list) {
    lxb_selectors_find(selectors, node, list, findAllPtrCallback, &results);
    lxb_css_selector_list_destroy_memory(list);
  }
  return results;
}

// ── Document structure ───────────────────────────────────────────────────────

void* LexborDocument::body() const {
  if (!_document) return nullptr;
  return lxb_html_document_body_element(static_cast<lxb_html_document_t*>(_document));
}

void* LexborDocument::head() const {
  if (!_document) return nullptr;
  return lxb_html_document_head_element(static_cast<lxb_html_document_t*>(_document));
}

void* LexborDocument::documentElement() const {
  if (!_document) return nullptr;
  return lxb_dom_document_element(
      lxb_dom_interface_document(static_cast<lxb_html_document_t*>(_document)));
}

// ── Script extraction ────────────────────────────────────────────────────────

std::vector<std::string> LexborDocument::getScriptContents() const {
  auto scriptEls = querySelectorAll_el("script");
  std::vector<std::string> contents;
  contents.reserve(scriptEls.size());

  for (void* el : scriptEls) {
    auto* node = lxb_dom_interface_node(static_cast<lxb_dom_element_t*>(el));
    size_t len = 0;
    lxb_char_t* text = lxb_dom_node_text_content(node, &len);
    if (text && len > 0) {
      contents.emplace_back(reinterpret_cast<char*>(text), len);
      lxb_dom_document_destroy_text(node->owner_document, text);
    }
  }
  return contents;
}

// ── Node creation ─────────────────────────────────────────────────────────────

void* LexborDocument::createElement(const std::string& tag) {
  if (!_document) return nullptr;
  auto* dom_doc = lxb_dom_interface_document(static_cast<lxb_html_document_t*>(_document));
  return lxb_dom_document_create_element(dom_doc,
      reinterpret_cast<const lxb_char_t*>(tag.data()), tag.size(), nullptr);
}

void* LexborDocument::createTextNode(const std::string& text) {
  if (!_document) return nullptr;
  auto* dom_doc = lxb_dom_interface_document(static_cast<lxb_html_document_t*>(_document));
  return lxb_dom_document_create_text_node(dom_doc,
      reinterpret_cast<const lxb_char_t*>(text.data()), text.size());
}

// ── Element content ──────────────────────────────────────────────────────────

void LexborDocument::setTextContentOnEl(void* element, const std::string& text) {
  auto* el   = static_cast<lxb_dom_element_t*>(element);
  auto* node = lxb_dom_interface_node(el);

  // Remove all existing children
  while (node->first_child) {
    lxb_dom_node_t* child = node->first_child;
    lxb_dom_node_remove(child);
    lxb_dom_node_destroy_deep(child);
  }

  if (text.empty()) return;

  auto* dom_doc   = node->owner_document;
  auto* text_node = lxb_dom_document_create_text_node(dom_doc,
      reinterpret_cast<const lxb_char_t*>(text.data()), text.size());
  if (text_node) {
    lxb_dom_node_insert_child(node, lxb_dom_interface_node(text_node));
  }
}

void LexborDocument::setInnerHTMLOnEl(void* element, const std::string& html) {
  auto* el  = static_cast<lxb_dom_element_t*>(element);
  auto* doc = static_cast<lxb_html_document_t*>(_document);
  auto* root = lxb_dom_interface_node(el);

  lxb_dom_node_t* frag = lxb_html_document_parse_fragment(doc, el,
      reinterpret_cast<const lxb_char_t*>(html.data()), html.size());
  if (!frag) return;

  while (root->first_child) lxb_dom_node_destroy_deep(root->first_child);

  while (frag->first_child) {
    lxb_dom_node_t* child = frag->first_child;
    lxb_dom_node_remove(child);
    lxb_dom_node_insert_child(root, child);
  }

  lxb_dom_node_destroy(frag);
}

// ── Selector matching ────────────────────────────────────────────────────────

bool LexborDocument::matchesSelector(void* element, const std::string& sel) const {
  auto results = querySelectorAll_el(sel);
  for (void* el : results) {
    if (el == element) return true;
  }
  return false;
}

// ── Legacy selector-based API ────────────────────────────────────────────────

std::optional<std::string> LexborDocument::querySelector(const std::string& selector) const {
  auto* el = static_cast<lxb_dom_element_t*>(findFirst(selector));
  if (!el) return std::nullopt;
  return serializeNode(lxb_dom_interface_node(el));
}

std::vector<std::string> LexborDocument::querySelectorAll(const std::string& selector) const {
  if (!_document) return {};
  auto* parser    = static_cast<lxb_css_parser_t*>(_cssParser);
  auto* selectors = static_cast<lxb_selectors_t*>(_selectors);
  auto* doc       = static_cast<lxb_html_document_t*>(_document);

  lxb_css_selector_list_t* list = lxb_css_selectors_parse(parser,
      reinterpret_cast<const lxb_char_t*>(selector.data()), selector.size());
  std::vector<std::string> results;
  if (list) {
    lxb_selectors_find(selectors, lxb_dom_interface_node(doc), list, findAllStringCallback, &results);
    lxb_css_selector_list_destroy_memory(list);
  }
  return results;
}

std::optional<std::string> LexborDocument::getAttribute(const std::string& selector, const std::string& attr) const {
  auto* el = static_cast<lxb_dom_element_t*>(findFirst(selector));
  if (!el) return std::nullopt;
  size_t val_len;
  const lxb_char_t* val = lxb_dom_element_get_attribute(el,
      reinterpret_cast<const lxb_char_t*>(attr.data()), attr.size(), &val_len);
  if (!val) return std::nullopt;
  return std::string(reinterpret_cast<const char*>(val), val_len);
}

void LexborDocument::setAttribute(const std::string& selector, const std::string& attr, const std::string& value) {
  auto* el = static_cast<lxb_dom_element_t*>(findFirst(selector));
  if (!el) return;
  lxb_dom_element_set_attribute(el,
      reinterpret_cast<const lxb_char_t*>(attr.data()), attr.size(),
      reinterpret_cast<const lxb_char_t*>(value.data()), value.size());
}

std::optional<std::string> LexborDocument::getTextContent(const std::string& selector) const {
  auto* el = static_cast<lxb_dom_element_t*>(findFirst(selector));
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
  auto* el = static_cast<lxb_dom_element_t*>(findFirst(selector));
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
  auto* el = static_cast<lxb_dom_element_t*>(findFirst(selector));
  if (!el) return;
  setInnerHTMLOnEl(el, html);
}

} // namespace margelo::nitro::nitrojsdom
