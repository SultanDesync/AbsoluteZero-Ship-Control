#include "Configuration.h"

#include "RuntimePaths.h"

#include <windows.h>

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
    config.centerOnPress =
        ReadInt("MouseAlign", "iActivationMode", defaults.centerOnPress ? 1 : 0, path) == 1;
    config.centerKey = ReadInt("MouseAlign", "iCenterKey", defaults.centerKey, path);
    config.enableLog = ReadBool("MouseAlign", "bLogEnabled", defaults.enableLog, path);
    config.virtualJoy = ReadBool("VirtualJoy", "bVirtualJoy", defaults.virtualJoy, path);
    config.vJoyToggleKey =
        ReadInt("VirtualJoy", "iVJoyToggleKey", defaults.vJoyToggleKey, path);
    config.vJoyToggleAlt =
        ReadBool("VirtualJoy", "bVJoyToggleAlt", defaults.vJoyToggleAlt, path);
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
               << "; Managed by AbsoluteZero; Absolute Workbench writes through the daughter API.\n\n"
               << "[MouseAlign]\n"
               << "bEnabled = " << Boolean(config.enabled) << '\n'
               << "fRadius = " << std::format("{:.3f}", config.radius) << '\n'
               << "iIdleMs = " << config.idleMs << '\n'
               << "fDecayRate = " << std::format("{:.3f}", config.decayRate) << '\n'
               << "iPollRateHz = " << config.pollRateHz << '\n'
               << std::format("iSuppressKey = 0x{:02X}\n", config.suppressKey)
               << "iActivationMode = " << (config.centerOnPress ? 1 : 0) << '\n'
               << std::format("iCenterKey = 0x{:02X}\n", config.centerKey)
               << "bLogEnabled = " << Boolean(config.enableLog) << "\n\n"
               << "[VirtualJoy]\n"
               << "bVirtualJoy = " << Boolean(config.virtualJoy) << '\n'
               << std::format("iVJoyToggleKey = 0x{:02X}\n", config.vJoyToggleKey)
               << "bVJoyToggleAlt = " << Boolean(config.vJoyToggleAlt) << '\n';
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
