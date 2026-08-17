#include "SuppressBindingCodec.h"

#include <charconv>
#include <format>

namespace SuppressBindingCodec {

bool Parse(std::string_view encoded, int& virtualKey) noexcept {
    if (encoded.empty()) {
        virtualKey = 0;
        return true;
    }

    constexpr std::string_view prefix = "keyboard:0x";
    constexpr std::string_view suffix = ";ctrl=0;alt=0;shift=0";
    if (!encoded.starts_with(prefix) || !encoded.ends_with(suffix)) {
        return false;
    }
    encoded.remove_prefix(prefix.size());
    encoded.remove_suffix(suffix.size());
    int parsed{};
    const auto [end, error] = std::from_chars(
        encoded.data(), encoded.data() + encoded.size(), parsed, 16);
    if (error != std::errc{} || end != encoded.data() + encoded.size() ||
        parsed <= 0 || parsed > 0xFF) {
        return false;
    }
    virtualKey = parsed;
    return true;
}

std::string Display(int virtualKey) {
    if (virtualKey <= 0) return "Unbound";
    if ((virtualKey >= 'A' && virtualKey <= 'Z') ||
        (virtualKey >= '0' && virtualKey <= '9')) {
        return std::string(1, static_cast<char>(virtualKey));
    }
    if (virtualKey >= 0x60 && virtualKey <= 0x69) {
        return std::format("Numpad {}", virtualKey - 0x60);
    }
    if (virtualKey >= 0x70 && virtualKey <= 0x87) {
        return std::format("F{}", virtualKey - 0x70 + 1);
    }
    switch (virtualKey) {
    case 0x08: return "Backspace";
    case 0x09: return "Tab";
    case 0x0D: return "Enter";
    case 0x14: return "Caps Lock";
    case 0x1B: return "Escape";
    case 0x20: return "Space";
    case 0x25: return "Left Arrow";
    case 0x26: return "Up Arrow";
    case 0x27: return "Right Arrow";
    case 0x28: return "Down Arrow";
    default: return std::format("Key 0x{:02X}", virtualKey);
    }
}

} // namespace SuppressBindingCodec
