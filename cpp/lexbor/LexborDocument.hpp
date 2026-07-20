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

  // ── Element content ───────────────────────────────────────────────────────
  void setTextContentOnEl(void* element, const std::string& text);
  void setInnerHTMLOnEl(void* element, const std::string& html);

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
