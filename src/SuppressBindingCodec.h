#pragma once

#include <string>
#include <string_view>

namespace SuppressBindingCodec {
// Absolute Control serializes keyboard captures in this format:
// keyboard:0x42;ctrl=0;alt=0;shift=0
// Keep the codec independent of a particular AbsoluteZero action so every
// user-configurable shortcut has the same capture and display behavior.
struct KeyboardBinding {
    int virtualKey{};
    bool control{};
    bool alt{};
    bool shift{};
};

[[nodiscard]] bool Parse(std::string_view encoded, KeyboardBinding& binding) noexcept;
[[nodiscard]] std::string Display(const KeyboardBinding& binding);
[[nodiscard]] std::string DisplayToken(std::string_view token);
[[nodiscard]] std::string Encode(const KeyboardBinding& binding);
} // namespace SuppressBindingCodec
