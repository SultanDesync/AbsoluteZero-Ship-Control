#include "SuppressBindingCodec.h"
#include "InputBusClient.h"

#include <charconv>
#include <format>

namespace SuppressBindingCodec {

bool Parse(std::string_view encoded, KeyboardBinding& binding) noexcept {
    if (encoded.empty()) {
        binding = {};
        return true;
    }

    constexpr std::string_view prefix = "keyboard:0x";
    if (!encoded.starts_with(prefix)) {
        return false;
    }
    encoded.remove_prefix(prefix.size());

    const auto modifiersAt = encoded.find(';');
    if (modifiersAt == std::string_view::npos) return false;
    const std::string_view keyText = encoded.substr(0, modifiersAt);
    std::string_view modifiers = encoded.substr(modifiersAt);
    int parsed{};
    const auto [end, error] = std::from_chars(
        keyText.data(), keyText.data() + keyText.size(), parsed, 16);
    if (error != std::errc{} || end != keyText.data() + keyText.size() ||
        parsed <= 0 || parsed > 0xFF) {
        return false;
    }

    auto parseModifier = [&](std::string_view prefix, bool& value) {
        if (!modifiers.starts_with(prefix)) return false;
        modifiers.remove_prefix(prefix.size());
        if (modifiers.empty() || (modifiers.front() != '0' && modifiers.front() != '1')) {
            return false;
        }
        value = modifiers.front() == '1';
        modifiers.remove_prefix(1);
        return true;
    };

    KeyboardBinding parsedBinding;
    if (!parseModifier(";ctrl=", parsedBinding.control) ||
        !parseModifier(";alt=", parsedBinding.alt) ||
        !parseModifier(";shift=", parsedBinding.shift) || !modifiers.empty()) {
        return false;
    }
    parsedBinding.virtualKey = parsed;
    binding = parsedBinding;
    return true;
}

std::string Display(const KeyboardBinding& binding) {
    if (binding.virtualKey <= 0) return "Unbound";

    std::string display;
    if (binding.control) display += "Ctrl + ";
    if (binding.alt) display += "Alt + ";
    if (binding.shift) display += "Shift + ";

    const int virtualKey = binding.virtualKey;
    if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9')) {
        return display + static_cast<char>(virtualKey);
    }
    if (virtualKey >= 0x60 && virtualKey <= 0x69) {
        return std::format("{}Numpad {}", display, virtualKey - 0x60);
    }
    if (virtualKey >= 0x70 && virtualKey <= 0x87) {
        return std::format("{}F{}", display, virtualKey - 0x70 + 1);
    }
    switch (virtualKey) {
    case 0x08: return display + "Backspace";
    case 0x09: return display + "Tab";
    case 0x0D: return display + "Enter";
    case 0x14: return display + "Caps Lock";
    case 0x1B: return display + "Escape";
    case 0x20: return display + "Space";
    case 0x25: return display + "Left Arrow";
    case 0x26: return display + "Up Arrow";
    case 0x27: return display + "Right Arrow";
    case 0x28: return display + "Down Arrow";
    default: return std::format("{}Key 0x{:02X}", display, virtualKey);
    }
}

std::string DisplayToken(std::string_view token) {
    if (token.empty() || token == "0" || token == "Unbound") {
        return "Unbound";
    }
    if (token.starts_with("keyboard:")) {
        KeyboardBinding binding;
        if (Parse(token, binding)) {
            return Display(binding);
        }
    }
    if (token.contains('@')) {
        return InputBusClient::Get().FormatBinding(token);
    }
    return std::string(token);
}

std::string Encode(const KeyboardBinding& binding) {
    if (binding.virtualKey <= 0) return "";
    return std::format("keyboard:0x{:02X};ctrl={};alt={};shift={}",
        binding.virtualKey,
        binding.control ? 1 : 0,
        binding.alt ? 1 : 0,
        binding.shift ? 1 : 0);
}

} // namespace SuppressBindingCodec
