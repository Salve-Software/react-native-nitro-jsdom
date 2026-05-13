#pragma once

#include <string>
#include <optional>
#include <vector>
#include <lexbor/html/html.h>

namespace margelo::nitro::nitrojsdom {

/**
 * Wraps a Lexbor HTML document.
 *
 * Lexbor provides a WHATWG-compliant HTML parser and in-memory DOM tree.
 */
class LexborDocument {
public:
  LexborDocument();
  ~LexborDocument();

  // Parse the given HTML string and build the internal DOM tree.
  void parse(const std::string& html);

  // Serialize the current DOM tree back to an HTML string.
  std::string serialize() const;

  // Return the serialized outer HTML of the first element matching selector, or nullopt.
  std::optional<std::string> querySelector(const std::string& selector) const;

  // Return serialized outer HTML for all elements matching selector.
  std::vector<std::string> querySelectorAll(const std::string& selector) const;

  // Attribute access on the first matching element.
  std::optional<std::string> getAttribute(const std::string& selector, const std::string& attr) const;
  void setAttribute(const std::string& selector, const std::string& attr, const std::string& value);

  // textContent / innerHTML on the first matching element.
  std::optional<std::string> getTextContent(const std::string& selector) const;
  std::optional<std::string> getInnerHTML(const std::string& selector) const;
  void setInnerHTML(const std::string& selector, const std::string& html);

private:
  lxb_html_document_t* _document { nullptr };

  lxb_dom_element_t* findFirst(const std::string& selector) const;
};

} // namespace margelo::nitro::nitrojsdom
