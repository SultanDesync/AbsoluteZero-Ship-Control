#include "MouseAlignHook.h"
#include "AlignmentAssist.h"
#include "RuntimePaths.h"
#include <SFSE/Interfaces.h>
#include <windows.h>
#include <format>

static bool IsFileLoggingEnabled() {
    return RuntimePaths::IsFileLoggingEnabled();
}

// Logging helper
static void InitializeLog() {
    if (!IsFileLoggingEnabled()) return;
    RuntimePaths::AppendLog("[AbsoluteZero]", "Started Log Session");
}

static void MainLog(const std::string& msg) {
    if (!IsFileLoggingEnabled()) return;
    RuntimePaths::AppendLog("[Main]", msg);
}

static LONG NTAPI CrashLogHandler(EXCEPTION_POINTERS* a_exceptionInfo) {
    if (!a_exceptionInfo || !a_exceptionInfo->ExceptionRecord || !a_exceptionInfo->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto* const record = a_exceptionInfo->ExceptionRecord;
    // Skip non-crash exception codes:
    // 0x40010006 = DBG_PRINTEXCEPTION_C (OutputDebugStringA)
    // 0x4001000A = DBG_PRINTEXCEPTION_WIDE_C (OutputDebugStringW)
    // 0x406D1388 = MS_VC_EXCEPTION (SetThreadName)
    // 0xE06D7363 = C++ throw — handled by SEH __try/__except in our render path
    if (record->ExceptionCode == 0x40010006 ||
        record->ExceptionCode == 0x4001000A ||
        record->ExceptionCode == 0x406D1388 ||
        record->ExceptionCode == 0xE06D7363) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto* const context = a_exceptionInfo->ContextRecord;
    MEMORY_BASIC_INFORMATION mbi{};
    char modulePath[MAX_PATH]{};
    const auto queryOk = VirtualQuery(record->ExceptionAddress, &mbi, sizeof(mbi)) != 0;
    if (queryOk) {
        GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), modulePath, static_cast<DWORD>(std::size(modulePath)));
    }

    RuntimePaths::AppendLogAlways("[Crash]",
        std::format("Exception code=0x{:08X} address=0x{:016X}",
            static_cast<std::uint32_t>(record->ExceptionCode),
            reinterpret_cast<std::uintptr_t>(record->ExceptionAddress)));
    if (queryOk) {
        RuntimePaths::AppendLogAlways("[Crash]",
            std::format("Module base=0x{:016X} rva=0x{:X} path={}",
                reinterpret_cast<std::uintptr_t>(mbi.AllocationBase),
                reinterpret_cast<std::uintptr_t>(record->ExceptionAddress) - reinterpret_cast<std::uintptr_t>(mbi.AllocationBase),
                modulePath[0] ? modulePath : "<unknown>"));
    }
    RuntimePaths::AppendLogAlways("[Crash]",
        std::format("RIP=0x{:016X} RSP=0x{:016X} RCX=0x{:016X} RDX=0x{:016X} R8=0x{:016X} R9=0x{:016X}",
            static_cast<std::uintptr_t>(context->Rip),
            static_cast<std::uintptr_t>(context->Rsp),
            static_cast<std::uintptr_t>(context->Rcx),
            static_cast<std::uintptr_t>(context->Rdx),
            static_cast<std::uintptr_t>(context->R8),
            static_cast<std::uintptr_t>(context->R9)));
    if (record->NumberParameters > 1) {
        RuntimePaths::AppendLogAlways("[Crash]",
            std::format("Exception info[0]=0x{:016X} info[1]=0x{:016X}",
                static_cast<std::uintptr_t>(record->ExceptionInformation[0]),
                static_cast<std::uintptr_t>(record->ExceptionInformation[1])));
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void InstallCrashLogger() {
    static void* handler = nullptr;
    if (!handler) {
        handler = AddVectoredExceptionHandler(1, CrashLogHandler);
        MainLog(handler ? "Crash logger installed." : "WARNING: Crash logger installation failed.");
    }
}

// Read alignment assist config from INI [MouseAlign] section
static AlignmentAssist::Config ReadConfig() {
    AlignmentAssist::Config cfg;
    auto ini = RuntimePaths::IniPath().string();
    const char* path = ini.c_str();

    auto readBool = [&](const char* key, bool def, const char* section = "MouseAlign") -> bool {
        char buf[32]{};
        GetPrivateProfileStringA(section, key, def ? "true" : "false", buf, sizeof(buf), path);
        std::string val(buf);
        for (auto& c : val) c = (char)tolower(c);
        return val == "true" || val == "1" || val == "yes" || val == "on";
    };

    auto readFloat = [&](const char* key, float def, const char* section = "MouseAlign") -> float {
        char buf[32]{};
        GetPrivateProfileStringA(section, key, std::to_string(def).c_str(), buf, sizeof(buf), path);
        try { return std::stof(buf); }
        catch (...) { return def; }
    };

    auto readInt = [&](const char* key, int def, const char* section = "MouseAlign") -> int {
        return GetPrivateProfileIntA(section, key, def, path);
    };

    cfg.enabled    = readBool("bEnabled", true);
    cfg.virtualJoy = readBool("bVirtualJoy", false, "VirtualJoy");
    cfg.vJoyToggleAlt = readBool("bVJoyToggleAlt", true, "VirtualJoy");
    cfg.radius     = readFloat("fRadius", 140.0f);
    cfg.idleMs     = readInt("iIdleMs", 80);
    cfg.decayRate  = readFloat("fDecayRate", 12.0f);
    cfg.pollRateHz = readInt("iPollRateHz", 120);

    // iSuppressKey: read as hex (e.g. "0x42") or decimal (e.g. "66")
    {
        char buf[32]{};
        GetPrivateProfileStringA("MouseAlign", "iSuppressKey", "0x42", buf, sizeof(buf), path);
        std::string val(buf);
        try {
            cfg.suppressKey = std::stoi(val, nullptr, 0);  // auto-detects 0x prefix
        } catch (...) {
            cfg.suppressKey = 0x42;  // B
        }
    }

    // iVJoyToggleKey: read as hex (e.g. "0x60") or decimal (e.g. "96")
    {
        char buf[32]{};
        GetPrivateProfileStringA("VirtualJoy", "iVJoyToggleKey", "0x60", buf, sizeof(buf), path);
        std::string val(buf);
        try {
            cfg.vJoyToggleKey = std::stoi(val, nullptr, 0);  // auto-detects 0x prefix
        } catch (...) {
            cfg.vJoyToggleKey = 0x60;  // Numpad 0
        }
    }

    return cfg;
}

// SFSE plugin entry point
SFSEPluginLoad(const SFSE::LoadInterface* /*a_sfse*/)
{
    // Read bLogEnabled from INI once and cache the result globally.
    RuntimePaths::EnableFileLogging();

    InitializeLog();
    InstallCrashLogger();

    // Startup banner always writes so the user can confirm the plugin loaded.
    RuntimePaths::AppendLogAlways("[Main]", "======================================================");
    RuntimePaths::AppendLogAlways("[Main]", "AbsoluteZero v0.1.0 - Mouse Alignment Assist");
    RuntimePaths::AppendLogAlways("[Main]", "Target: Starfield (Version Independent)");
    RuntimePaths::AppendLogAlways("[Main]", "======================================================");

    // Compatibility guard: do not co-load with AbsoluteHOTAS (both hook the same writer block)
    if (GetModuleHandleA("AbsoluteHOTAS.dll")) {
        RuntimePaths::AppendLogAlways("[Main]",
            "WARNING: AbsoluteHOTAS.dll detected. AbsoluteZero is not needed when AbsoluteHOTAS is loaded.");
        RuntimePaths::AppendLogAlways("[Main]",
            "Use AbsoluteHOTAS's built-in bAlignmentAssist option instead. AbsoluteZero will not install hooks.");
        return true;  // Return true so SFSE doesn't report a load failure
    }

    // Read config
    auto config = ReadConfig();
    MainLog(std::format("Config loaded: enabled={} vJoy={} radius={:.1f} idleMs={} decayRate={:.1f} pollHz={}",
        config.enabled, config.virtualJoy, config.radius, config.idleMs, config.decayRate, config.pollRateHz));

    // Phase 1: AOB scan + trampoline hook
    bool hookOk = MouseAlignHook::Install();
    if (!hookOk) {
        MainLog("WARNING: Hook installation failed. Alignment assist disabled.");
        RuntimePaths::AppendLogAlways("[Main]",
            "Hook installation failed. The game may have updated — check AbsoluteZero.log for details.");
        return true;  // Don't report SFSE failure
    }

    // Phase 2: Start alignment assist polling thread
    MainLog("Hook installed. Starting alignment assist.");
    AlignmentAssist::Initialize(config);
    AlignmentAssist::Start();
    MainLog("AlignmentAssist started successfully.");

    RuntimePaths::AppendLogAlways("[Main]", "Plugin load complete.");
    return true;
}
