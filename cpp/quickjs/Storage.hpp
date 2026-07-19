#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstddef>

// Forward-declare the opaque JSContext/JSValue types to avoid including quickjs.h here
typedef struct JSContext JSContext;

namespace margelo::nitro::nitrojsdom {

// ── Storage ───────────────────────────────────────────────────────────────────
//
// In-memory backing store for `localStorage` / `sessionStorage`. Preserves
// insertion order (per the Web Storage spec's `key(index)` iteration order);
// re-setting an existing key updates its value in place without moving it.
// Purely in-memory — not persisted across JSDOM instances or app restarts.

class Storage {
public:
  std::optional<std::string> getItem(const std::string& key) const;
  void setItem(const std::string& key, const std::string& value);
  void removeItem(const std::string& key);
  void clear();
  std::optional<std::string> key(size_t index) const;
  size_t length() const { return _order.size(); }

private:
  std::vector<std::string> _order;
  std::unordered_map<std::string, std::string> _data;
};

// Registers `getItem`/`setItem`/`removeItem`/`clear`/`key`/`length` on a new JS
// object bound to `globalName` on the given context, backed by `storage`.
// `storage` must outlive the QuickJS context (owned by RuntimeContext).
void installStorage(JSContext* ctx, const char* globalName, Storage* storage);

} // namespace margelo::nitro::nitrojsdom
