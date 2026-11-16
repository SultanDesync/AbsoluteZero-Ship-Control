#include "AlignmentAssist.h"
#include "Configuration.h"
#include "HotasMouseCompatibility.h"
#include "MouseAlignHook.h"
#include "RuntimePaths.h"
#include "SettingsPage.h"
#include "SuiteState.h"

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

extern "C" void VirtualJoySync();

void InitializeRuntime() {
    if (g_runtimeInitialized.exchange(true)) {
        return;
    }
    const bool hotasPresent = AbsoluteHOTASPresent();
    if (hotasPresent) {
        const auto connection = HotasMouseCompatibility::Connect();
        if (connection == HotasMouseCompatibility::Connection::Connected) {
            MouseAlignHook::SetExternalAccumulatorProvider(
                &HotasMouseCompatibility::ReadMouseAccumulator,
                &HotasMouseCompatibility::WriteMouseAccumulator);
            SuiteState::SetInactiveByHOTAS(false);
            SuiteState::SetHookInstalled(false);
            SuiteState::SetRuntimeReady(true);
            if (HotasMouseCompatibility::SupportsVirtualJoy()) {
                HotasMouseCompatibility::RegisterWriterSyncCallback(&VirtualJoySync);
                RuntimePaths::AppendLogAlways(
                    "[Compatibility]",
                    "AbsoluteHOTAS accepted AbsoluteZero pitch/yaw ownership with ApiV2 "
                    "synchronized writer-gate VJoy authority.");
            } else {
                if (Configuration::Get().virtualJoy) {
                    RuntimePaths::AppendLogAlways(
                        "[Compatibility]",
                        "Legacy VirtualJoy is unavailable while sharing the HOTAS writer; native "
                        "mouse auto-centering remains active.");
                }
                RuntimePaths::AppendLogAlways(
                    "[Compatibility]",
                    "AbsoluteHOTAS accepted AbsoluteZero pitch/yaw ownership (ApiV1). Using HOTAS's "
                    "shared accumulator seam without installing a second writer hook.");
            }
            AlignmentAssist::Start();
            return;
        }

        SuiteState::SetInactiveByHOTAS(true);
        SuiteState::SetRuntimeReady(false);
        RuntimePaths::AppendLogAlways(
            "[Compatibility]",
            std::format(
                "AbsoluteHOTAS was detected but did not provide the compatible shared mouse "
                "capture contract (connection {}). AbsoluteZero remains fail-closed.",
                static_cast<int>(connection)));
        return;
    }

    SuiteState::SetInactiveByHOTAS(false);

    const bool hookInstalled = MouseAlignHook::Install();
    SuiteState::SetHookInstalled(hookInstalled);
    SuiteState::SetRuntimeReady(hookInstalled);
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
    if (!message) return;
    if (message->type == SFSE::MessagingInterface::kPostPostLoad) {
        InitializeRuntime();
    }
}

bool RegisterSfseListener(const SFSE::LoadInterface* sfse) noexcept {
    if (!sfse) {
        return false;
    }
    const auto* load = reinterpret_cast<const SFSE::detail::SFSEInterface*>(sfse);
    if (!load->QueryInterface || !load->GetPluginHandle) {
        return false;
    }
    const auto* messaging = static_cast<const SFSE::detail::SFSEMessagingInterface*>(
        load->QueryInterface(SFSE::LoadInterface::kMessaging));
    return messaging && messaging->interfaceVersion >= SFSE::MessagingInterface::kVersion &&
           messaging->RegisterListener &&
           messaging->RegisterListener(load->GetPluginHandle(), "SFSE",
                                       reinterpret_cast<void*>(&OnSfseMessage));
}
} // namespace

SFSEPluginLoad(const SFSE::LoadInterface* sfse) {
    RuntimePaths::EnableFileLogging();
    InstallCrashLogger();
    RuntimePaths::AppendLogAlways("[Main]", "AbsoluteZero 1.5.0 startup.");
    Configuration::Load();

    if (!sfse) {
        RuntimePaths::AppendLogAlways(
            "[Main]", "SFSE load interface is null; runtime hook installation is suppressed.");
        return true;
    }
    if (!RegisterSfseListener(sfse)) {
        RuntimePaths::AppendLogAlways(
            "[Main]",
            "Could not subscribe to SFSE post-post-load messaging; hook installation is "
            "suppressed to make AbsoluteHOTAS compatibility load-order independent.");
        return true;
    }
    RuntimePaths::AppendLog("[Main]", "Configuration API ready; runtime initialization deferred.");

    // The settings page is optional. With bMenuEnabled = false no renderer hook exists.
    if (!Configuration::Get().menuEnabled) {
        RuntimePaths::AppendLog("[Main]", "Settings page disabled by bMenuEnabled.");
    } else if (!SettingsPage::Install()) {
        RuntimePaths::AppendLog(
            "[Main]", "Settings page unavailable; INI and hotkeys still work.");
    }
    return true;
}
