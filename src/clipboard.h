#pragma once

#include <string>
#include <string_view>

namespace clipboard {

// Copies to the OS clipboard when possible and always keeps an internal copy
// so the app works even on backends without clipboard support.
bool set(std::string_view text);
std::string get();

}  // namespace clipboard
