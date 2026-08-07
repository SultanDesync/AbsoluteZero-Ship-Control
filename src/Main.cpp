#include "AlignmentAssist.h"
#include "Configuration.h"
#include "MouseAlignHook.h"
#include "RuntimePaths.h"
#include "SuiteState.h"
#include "WorkbenchHost.h"

#include <SFSE/Interfaces.h>
#include <windows.h>

#include <atomic>
#include <filesystem>
#include <format>

namespace {
std::atomic<bool> g_runtimeInitialized{};

LONG NTAPI CrashLogHandler(EXCEPTION_POINTERS* exceptionInfo) {
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto* record = exceptionInfo->ExceptionRecord;
    if (record->ExceptionCode == 0x40010006 || record->ExceptionCode == 0x4001000A ||
        record->ExceptionCode == 0x406D1388 || record->ExceptionCode == 0xE06D7363) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    RuntimePaths::AppendLogAlways(
        "[Crash]",
        std::format("Exception code=0x{:08X} address=0x{:016X}",
                    static_cast<std::uint32_t>(record->ExceptionCode),
                    reinterpret_cast<std::uintptr_t>(record->ExceptionAddress)));
    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashLogger() {
    static void* handler{};
    if (!handler) {
        handler = AddVectoredExceptionHandler(1, CrashLogHandler);
    }
}

bool AbsoluteHOTASPresent() {
    std::error_code error;
    return GetModuleHandleW(L"AbsoluteHOTAS.dll") != nullptr ||
           std::filesystem::exists(RuntimePaths::PluginDirectory() / L"AbsoluteHOTAS.dll", error);
}

void InitializeRuntime() {
    if (g_runtimeInitialized.exchange(true)) {
        return;
    }
    const auto selection = WorkbenchHost::Select();
    SuiteState::SetHostActive(selection == WorkbenchHost::Selection::Active);

    const bool inactiveByHOTAS = AbsoluteHOTASPresent();
    SuiteState::SetInactiveByHOTAS(inactiveByHOTAS);
    if (inactiveByHOTAS) {
        RuntimePaths::AppendLogAlways(
            "[Compatibility]",
            "AbsoluteHOTAS.dll detected by loaded module or installed file. AbsoluteZero remains "
            "queryable for diagnostics but installs no writer hook.");
        return;
    }

    const bool hookInstalled = MouseAlignHook::Install();
    SuiteState::SetHookInstalled(hookInstalled);
    if (!hookInstalled) {
        RuntimePaths::AppendLogAlways(
            "[Main]",
            "Rotational writer hook did not pass its signature gate. Alignment remains disabled.");
        return;
    }
    AlignmentAssist::Start();
    RuntimePaths::AppendLog("[Main]", "Alignment worker started after post-post-load selection.");
}

void OnSfseMessage(SFSE::MessagingInterface::Message* message) {
    if (message && message->type == SFSE::MessagingInterface::kPostPostLoad) {
        InitializeRuntime();
    }
}
} // namespace

SFSEPluginLoad(const SFSE::LoadInterface* sfse) {
    RuntimePaths::EnableFileLogging();
    InstallCrashLogger();
    RuntimePaths::AppendLogAlways("[Main]", "AbsoluteZero 0.2.0-alpha startup.");
    Configuration::Load();

    if (!sfse) {
        RuntimePaths::AppendLogAlways(
            "[Main]", "SFSE load interface is null; runtime hook installation is suppressed.");
        return true;
    }
    SFSE::Init(sfse, false);
    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging || messaging->Version() < SFSE::MessagingInterface::kVersion ||
        !messaging->RegisterListener(&OnSfseMessage)) {
        RuntimePaths::AppendLogAlways(
            "[Main]",
            "Could not subscribe to SFSE post-post-load messaging; hook installation is "
            "suppressed to make AbsoluteHOTAS compatibility load-order independent.");
        return true;
    }
    RuntimePaths::AppendLog("[Main]", "Configuration API ready; runtime initialization deferred.");
    return true;
}
