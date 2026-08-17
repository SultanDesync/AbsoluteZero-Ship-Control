#pragma once

#include <string>
#include <string_view>

namespace SuppressBindingCodec {
[[nodiscard]] bool Parse(std::string_view encoded, int& virtualKey) noexcept;
[[nodiscard]] std::string Display(int virtualKey);
} // namespace SuppressBindingCodec
