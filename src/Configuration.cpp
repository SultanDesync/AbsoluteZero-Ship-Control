#include "Configuration.h"
#include "SuppressBindingCodec.h"
#include "RuntimePaths.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>

namespace {
std::mutex g_mutex;
Configuration::Config g_config = ConfigPolicy::Defaults();
bool g_dirty{};
std::uint64_t g_generation{};

bool ReadBool(const char* section, const char* key, bool fallback, const std::string& path) {
    char value[32]{};
    GetPrivateProfileStringA(section, key, fallback ? "true" : "false", value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    std::string normalized(value);
    std::ranges::transform(normalized, normalized.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return normalized == "true" || normalized == "1" || normalized == "yes" ||
           normalized == "on";
}

float ReadFloat(const char* section, const char* key, float fallback, const std::string& path) {
    char value[64]{};
    const std::string defaultValue = std::to_string(fallback);
    GetPrivateProfileStringA(section, key, defaultValue.c_str(), value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    try {
        return std::stof(value);
    } catch (...) {
        return fallback;
    }
}

int ReadInt(const char* section, const char* key, int fallback, const std::string& path) {
    char value[64]{};
    const std::string defaultValue = std::to_string(fallback);
    GetPrivateProfileStringA(section, key, defaultValue.c_str(), value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    try {
        return std::stoi(value, nullptr, 0);
    } catch (...) {
        return fallback;
    }
}

std::string ReadString(const char* section, const char* key, const std::string& fallback, const std::string& path) {
    char value[256]{};
    GetPrivateProfileStringA(section, key, fallback.c_str(), value,
                             static_cast<DWORD>(std::size(value)), path.c_str());
    return std::string(value);
}

Configuration::Config ReadConfiguration() {
    const auto defaults = ConfigPolicy::Defaults();
    const std::string path = RuntimePaths::IniPath().string();
    Configuration::Config config;
    config.enabled = ReadBool("MouseAlign", "bEnabled", defaults.enabled, path);
    config.radius = ReadFloat("MouseAlign", "fRadius", defaults.radius, path);
    config.idleMs = ReadInt("MouseAlign", "iIdleMs", defaults.idleMs, path);
    config.decayRate = ReadFloat("MouseAlign", "fDecayRate", defaults.decayRate, path);
    config.pollRateHz = ReadInt("MouseAlign", "iPollRateHz", defaults.pollRateHz, path);
    config.suppressKey = ReadInt("MouseAlign", "iSuppressKey", defaults.suppressKey, path);
    config.suppressControl =
        ReadBool("MouseAlign", "bSuppressKeyCtrl", defaults.suppressControl, path);
    config.suppressAlt = ReadBool("MouseAlign", "bSuppressKeyAlt", defaults.suppressAlt, path);
    config.suppressShift =
        ReadBool("MouseAlign", "bSuppressKeyShift", defaults.suppressShift, path);
    config.suppressBinding = ReadString("MouseAlign", "sSuppressBinding", "", path);
    if (config.suppressBinding.empty()) {
        config.suppressBinding = SuppressBindingCodec::Encode({
            config.suppressKey, config.suppressControl, config.suppressAlt, config.suppressShift
        });
    } else {
        SuppressBindingCodec::KeyboardBinding kb;
        if (SuppressBindingCodec::Parse(config.suppressBinding, kb)) {
            config.suppressKey = kb.virtualKey;
            config.suppressControl = kb.control;
            config.suppressAlt = kb.alt;
            config.suppressShift = kb.shift;
        } else if (config.suppressBinding.contains('@')) {
            config.suppressKey = 0;
            config.suppressControl = false;
            config.suppressAlt = false;
            config.suppressShift = false;
        }
    }

    config.centerOnPress =
        ReadInt("MouseAlign", "iActivationMode", defaults.centerOnPress ? 1 : 0, path) == 1;
    config.centerKey = ReadInt("MouseAlign", "iCenterKey", defaults.centerKey, path);
    config.centerControl =
        ReadBool("MouseAlign", "bCenterKeyCtrl", defaults.centerControl, path);
    config.centerAlt = ReadBool("MouseAlign", "bCenterKeyAlt", defaults.centerAlt, path);
    config.centerShift =
        ReadBool("MouseAlign", "bCenterKeyShift", defaults.centerShift, path);
    config.centerBinding = ReadString("MouseAlign", "sCenterBinding", "", path);
    if (config.centerBinding.empty()) {
        config.centerBinding = SuppressBindingCodec::Encode({
            config.centerKey, config.centerControl, config.centerAlt, config.centerShift
        });
    } else {
        SuppressBindingCodec::KeyboardBinding kb;
        if (SuppressBindingCodec::Parse(config.centerBinding, kb)) {
            config.centerKey = kb.virtualKey;
            config.centerControl = kb.control;
            config.centerAlt = kb.alt;
            config.centerShift = kb.shift;
        } else if (config.centerBinding.contains('@')) {
            config.centerKey = 0;
            config.centerControl = false;
            config.centerAlt = false;
            config.centerShift = false;
        }
    }

    config.enableLog = ReadBool("MouseAlign", "bLogEnabled", defaults.enableLog, path);
    config.virtualJoy = ReadBool("VirtualJoy", "bVirtualJoy", defaults.virtualJoy, path);
    config.vJoyToggleKey =
        ReadInt("VirtualJoy", "iVJoyToggleKey", defaults.vJoyToggleKey, path);
    config.vJoyToggleControl =
        ReadBool("VirtualJoy", "bVJoyToggleCtrl", defaults.vJoyToggleControl, path);
    config.vJoyToggleAlt =
        ReadBool("VirtualJoy", "bVJoyToggleAlt", defaults.vJoyToggleAlt, path);
    config.vJoyToggleShift =
        ReadBool("VirtualJoy", "bVJoyToggleShift", defaults.vJoyToggleShift, path);
    config.vJoyToggleBinding = ReadString("VirtualJoy", "sVJoyToggleBinding", "", path);
    if (config.vJoyToggleBinding.empty()) {
        config.vJoyToggleBinding = SuppressBindingCodec::Encode({
            config.vJoyToggleKey, config.vJoyToggleControl, config.vJoyToggleAlt, config.vJoyToggleShift
        });
    } else {
        SuppressBindingCodec::KeyboardBinding kb;
        if (SuppressBindingCodec::Parse(config.vJoyToggleBinding, kb)) {
            config.vJoyToggleKey = kb.virtualKey;
            config.vJoyToggleControl = kb.control;
            config.vJoyToggleAlt = kb.alt;
            config.vJoyToggleShift = kb.shift;
        } else if (config.vJoyToggleBinding.contains('@')) {
            config.vJoyToggleKey = 0;
            config.vJoyToggleControl = false;
            config.vJoyToggleAlt = false;
            config.vJoyToggleShift = false;
        }
    }

    return ConfigPolicy::Sanitize(config);
}

std::string Boolean(bool value) { return value ? "true" : "false"; }

bool WriteAtomically(const Configuration::Config& config) {
    const auto destination = RuntimePaths::IniPath();
    auto temporary = destination;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }
        stream << "; AbsoluteZero.ini - Mouse Alignment Assist for Starfield\n"
               << "; Managed by AbsoluteZero; Absolute Control writes through the subscriber API.\n\n"
               << "[MouseAlign]\n"
               << "bEnabled = " << Boolean(config.enabled) << '\n'
               << "fRadius = " << std::format("{:.3f}", config.radius) << '\n'
               << "iIdleMs = " << config.idleMs << '\n'
               << "fDecayRate = " << std::format("{:.3f}", config.decayRate) << '\n'
               << "iPollRateHz = " << config.pollRateHz << '\n'
               << std::format("iSuppressKey = 0x{:02X}\n", config.suppressKey)
               << "bSuppressKeyCtrl = " << Boolean(config.suppressControl) << '\n'
               << "bSuppressKeyAlt = " << Boolean(config.suppressAlt) << '\n'
               << "bSuppressKeyShift = " << Boolean(config.suppressShift) << '\n'
               << "sSuppressBinding = " << config.suppressBinding << '\n'
               << "iActivationMode = " << (config.centerOnPress ? 1 : 0) << '\n'
               << std::format("iCenterKey = 0x{:02X}\n", config.centerKey)
               << "bCenterKeyCtrl = " << Boolean(config.centerControl) << '\n'
               << "bCenterKeyAlt = " << Boolean(config.centerAlt) << '\n'
               << "bCenterKeyShift = " << Boolean(config.centerShift) << '\n'
               << "sCenterBinding = " << config.centerBinding << '\n'
               << "bLogEnabled = " << Boolean(config.enableLog) << "\n\n"
               << "[VirtualJoy]\n"
               << "bVirtualJoy = " << Boolean(config.virtualJoy) << '\n'
               << std::format("iVJoyToggleKey = 0x{:02X}\n", config.vJoyToggleKey)
               << "bVJoyToggleCtrl = " << Boolean(config.vJoyToggleControl) << '\n'
               << "bVJoyToggleAlt = " << Boolean(config.vJoyToggleAlt) << '\n'
               << "bVJoyToggleShift = " << Boolean(config.vJoyToggleShift) << '\n'
               << "sVJoyToggleBinding = " << config.vJoyToggleBinding << '\n';
        stream.flush();
        if (!stream) {
            std::error_code error;
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}
} // namespace

namespace Configuration {
void Load() {
    const Config loaded = ReadConfiguration();
    std::scoped_lock lock(g_mutex);
    g_config = loaded;
    g_dirty = false;
    ++g_generation;
}

Config Get() {
    std::scoped_lock lock(g_mutex);
    return g_config;
}

bool Apply(const Config& config) {
    if (!ConfigPolicy::HasFiniteNumbers(config)) {
        return false;
    }
    const Config sanitized = ConfigPolicy::Sanitize(config);
    std::scoped_lock lock(g_mutex);
    if (sanitized != g_config) {
        g_config = sanitized;
        g_dirty = true;
        ++g_generation;
    }
    return true;
}

bool Save() {
    Config snapshot;
    {
        std::scoped_lock lock(g_mutex);
        snapshot = g_config;
    }
    if (!WriteAtomically(snapshot)) {
        return false;
    }
    std::scoped_lock lock(g_mutex);
    if (snapshot == g_config) {
        g_dirty = false;
    }
    return true;
}

bool Dirty() {
    std::scoped_lock lock(g_mutex);
    return g_dirty;
}

std::uint64_t Generation() {
    std::scoped_lock lock(g_mutex);
    return g_generation;
}
} // namespace Configuration
