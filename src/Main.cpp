#include "AlignmentAssist.h"
#include "Configuration.h"
#include "ControlPanelDiscovery.h"
#include "ControlPanelSubscriber.h"
#include "HotasMouseCompatibility.h"
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
std::atomic<bool> g_frontendSelected{};

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
            if (Configuration::Get().virtualJoy) {
                RuntimePaths::AppendLogAlways(
                    "[Compatibility]",
                    "Legacy VirtualJoy is unavailable while sharing the HOTAS writer; native "
                    "mouse auto-centering remains active.");
            }
            AlignmentAssist::Start();
            RuntimePaths::AppendLogAlways(
                "[Compatibility]",
                "AbsoluteHOTAS accepted AbsoluteZero pitch/yaw ownership. Using HOTAS's shared "
                "accumulator seam without installing a second writer hook.");
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

void SelectFallbackFrontend() {
    if (g_frontendSelected.exchange(true)) return;
    const auto selection = WorkbenchHost::Select();
    SuiteState::SetHostActive(selection == WorkbenchHost::Selection::Active);
}

void SelectFrontend(bool finalAttempt) {
    if (g_frontendSelected.load()) return;
    const auto result = ControlPanelSubscriber::RegisterDiscoveredHost();
    using ControlPanelSubscriber::Discovery::RegistrationDecision;
    const auto decision = ControlPanelSubscriber::Discovery::DecideRegistration(
        result, finalAttempt);
    if (decision == RegistrationDecision::UseControlPanel) {
        g_frontendSelected.store(true);
        SuiteState::SetHostActive(true);
        RuntimePaths::AppendLogAlways(
            "[ControlPanel]",
            "Registered the current Mouse Alignment page with Absolute Control Panel.");
        return;
    }
    if (decision == RegistrationDecision::RetryAtPostPostDataLoad) {
        RuntimePaths::AppendLogAlways(
            "[ControlPanel]",
            "Absolute Control Panel is not ready; registration will retry at "
            "post-post-data-load.");
        return;
    }
    RuntimePaths::AppendLogAlways(
        "[ControlPanel]",
        result == AbsoluteControlPanelApi::Result::NotFound
            ? "Absolute Control Panel is not installed; selecting the legacy Workbench fallback."
            : std::format(
                  "Absolute Control Panel registration was unavailable (result {}); selecting "
                  "the legacy Workbench fallback.",
                  static_cast<std::uint32_t>(result)));
    SelectFallbackFrontend();
}

void OnSfseMessage(SFSE::MessagingInterface::Message* message) {
    if (!message) return;
    if (message->type == SFSE::MessagingInterface::kPostPostLoad) {
        InitializeRuntime();
    } else if (message->type == SFSE::MessagingInterface::kPostDataLoad ||
               message->type == SFSE::MessagingInterface::kPostPostDataLoad) {
        SelectFrontend(message->type == SFSE::MessagingInterface::kPostPostDataLoad);
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
    RuntimePaths::AppendLogAlways("[Main]", "AbsoluteZero 1.3.0 startup.");
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
    return true;
}
