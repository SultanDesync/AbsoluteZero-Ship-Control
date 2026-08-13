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

    // Gated by bLogEnabled. When disabled, no file is opened or created.
    void AppendLog(const char* a_prefix, const std::string& a_message);

    // Deprecated alias for AppendLog. Kept for call-site compatibility; it is
    // now also gated by bLogEnabled (no longer "always"). Prefer AppendLog.
    void AppendLogAlways(const char* a_prefix, const std::string& a_message);
}
