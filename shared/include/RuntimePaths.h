#pragma once

#include <filesystem>
#include <string>

namespace RuntimePaths {
    std::filesystem::path PluginDirectory();
    std::filesystem::path IniPath();
    std::filesystem::path LogPath();

    bool IsFileLoggingEnabled();

    // Call once at startup to read bLogEnabled from INI and set the global flag.
    void EnableFileLogging();

    // Gated by bLogEnabled — use for all runtime/per-frame messages.
    void AppendLog(const char* a_prefix, const std::string& a_message);

    // Always writes regardless of bLogEnabled — use for startup/init only.
    void AppendLogAlways(const char* a_prefix, const std::string& a_message);
}
