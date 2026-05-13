#pragma once

#include <string>
#include <optional>
#include <vector>

namespace margelo::nitro::nitrojsdom {

class LexborDocument {
public:
  LexborDocument();
  ~LexborDocument();

  void parse(const std::string& html);
  std::string serialize() const;

  std::optional<std::string> querySelector(const std::string& selector) const;
  std::vector<std::string> querySelectorAll(const std::string& selector) const;

  std::optional<std::string> getAttribute(const std::string& selector, const std::string& attr) const;
  void setAttribute(const std::string& selector, const std::string& attr, const std::string& value);

  std::optional<std::string> getTextContent(const std::string& selector) const;
  std::optional<std::string> getInnerHTML(const std::string& selector) const;
  void setInnerHTML(const std::string& selector, const std::string& html);

private:
  void* _document  { nullptr };
  void* _cssParser { nullptr };
  void* _selectors { nullptr };

  void* findFirst(const std::string& selector) const;
};

} // namespace margelo::nitro::nitrojsdom
