#pragma once

#include <string>
#include <optional>
#include <vector>

namespace margelo::nitro::nitrojsdom {

/**
 * Wraps a Lexbor HTML document.
 *
 * Lexbor provides a WHATWG-compliant HTML parser and in-memory DOM tree.
 * Integration: add Lexbor sources to third_party/lexbor and link via CMakeLists.txt.
 * Include <lexbor/html/html.h> and replace stub bodies with real Lexbor calls.
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
  // Opaque pointer to lxb_html_document_t — will be a real type once Lexbor is linked.
  void* _document { nullptr };

  // Stored HTML used by the stub serializer until Lexbor is wired up.
  std::string _rawHtml;
};

} // namespace margelo::nitro::nitrojsdom
