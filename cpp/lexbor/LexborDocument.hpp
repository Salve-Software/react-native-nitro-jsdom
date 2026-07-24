#pragma once

#include <string>
#include <vector>

namespace margelo::nitro::nitrojsdom {

class LexborDocument {
public:
  LexborDocument();
  ~LexborDocument();

  // ── Parse / Serialize ──────────────────────────────────────────────────────
  void parse(const std::string& html);
  std::string serialize() const;

  // ── Raw document pointer (void* = lxb_html_document_t*) ──────────────────
  void* documentHtmlPtr() const { return _document; }

  // ── Element queries — return void* (lxb_dom_element_t*) ──────────────────
  void* getElementById(const std::string& id) const;
  void* querySelector_el(const std::string& sel) const;
  std::vector<void*> querySelectorAll_el(const std::string& sel) const;
  void* querySelectorFromEl(void* element, const std::string& sel) const;
  std::vector<void*> querySelectorAllFromEl(void* element, const std::string& sel) const;

  // ── Document structure ────────────────────────────────────────────────────
  void* body() const;
  void* head() const;
  void* documentElement() const;

  // ── Script extraction ────────────────────────────────────────────────────
  std::vector<std::string> getScriptContents() const;

  // ── Node creation ─────────────────────────────────────────────────────────
  void* createElement(const std::string& tag);
  void* createTextNode(const std::string& text);
  void* createComment(const std::string& text);
  void* createDocumentFragment();

  // ── Namespaces (createElementNS / Element.namespaceURI) ───────────────────
  // qualifiedName may be "prefix:localName" or a bare "localName". nsUri may be
  // empty for the null namespace. Lexbor registers arbitrary namespace URIs
  // dynamically (lxb_ns_append), so any URI works, not just HTML/SVG/MathML.
  // Note: lxb_tag_append_lower lowercases local_name internally, so
  // mixed-case foreign tag/attribute names (e.g. SVG's "viewBox",
  // "linearGradient") come back lowercased — a lexbor limitation, not
  // something this binding can work around.
  void* createElementNS(const std::string& nsUri, const std::string& qualifiedName);

  // Returns the namespace URI for a node, or "" if the node has no namespace
  // (LXB_NS__UNDEF) — callers should map "" to JS null.
  std::string namespaceURI(void* node) const;

  // ── Shadow DOM ─────────────────────────────────────────────────────────────
  // mode: 0 = open, 1 = closed (matches lxb_dom_shadow_root_mode_t).
  void* createShadowRoot(void* hostElement, int mode);

  // ── <template> ─────────────────────────────────────────────────────────────
  // Returns the lxb_dom_document_fragment_t* backing a <template> element's
  // `content`. Lexbor allocates this at element-creation time (both for parsed
  // and createElement()'d templates), so this is a plain field read.
  void* templateContent(void* templateEl) const;

  // ── Doctype ────────────────────────────────────────────────────────────────
  // Returns the lxb_dom_document_type_t* for this document, or nullptr if the
  // parsed HTML had no doctype.
  void* doctype() const;
  std::string doctypeName(void* doctype) const;
  std::string doctypePublicId(void* doctype) const;
  std::string doctypeSystemId(void* doctype) const;

  // ── Node.normalize() ───────────────────────────────────────────────────────
  // Merges adjacent Text node siblings and removes empty ones, recursively
  // through node's subtree (node itself is not replaced, only its descendants).
  void normalize(void* node);

  // ── Node.compareDocumentPosition() ────────────────────────────────────────
  // Returns the DOM DOCUMENT_POSITION_* bitmask describing nodeB's position
  // relative to nodeA (mirrors `nodeA.compareDocumentPosition(nodeB)`).
  int compareDocumentPosition(void* nodeA, void* nodeB) const;

  // ── Element content ───────────────────────────────────────────────────────
  void setTextContentOnEl(void* element, const std::string& text);
  void setInnerHTMLOnEl(void* element, const std::string& html);
  void setInnerHTMLOnShadowRoot(void* shadowRoot, void* hostElement, const std::string& html);

  // Like setInnerHTMLOnEl, but returns the detached parsed nodes instead of
  // replacing contextElement's children — used by insertAdjacentHTML.
  std::vector<void*> parseFragmentNodes(void* contextElement, const std::string& html);

  // ── Selector matching ─────────────────────────────────────────────────────
  bool matchesSelector(void* element, const std::string& sel) const;

private:
  void* _document  { nullptr };
  void* _cssParser { nullptr };
  void* _selectors { nullptr };

  void* findFirst(const std::string& selector) const;
  void* findFirstFrom(void* startNode, const std::string& selector) const;
};

} // namespace margelo::nitro::nitrojsdom
