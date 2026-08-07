#include "AbsoluteZeroAPI.h"

#include "Configuration.h"
#include "MouseAlignHook.h"
#include "SuiteState.h"

#include <cmath>

namespace {
using namespace AbsoluteZeroApi;

ConfigurationV1 ToApi(const Configuration::Config& config) {
    ConfigurationV1 output{};
    output.enabled = config.enabled ? 1 : 0;
    output.virtualJoy = config.virtualJoy ? 1 : 0;
    output.vJoyToggleAlt = config.vJoyToggleAlt ? 1 : 0;
    output.enableLog = config.enableLog ? 1 : 0;
    output.vJoyToggleKey = config.vJoyToggleKey;
    output.radius = config.radius;
    output.idleMilliseconds = config.idleMs;
    output.decayRate = config.decayRate;
    output.pollRateHz = config.pollRateHz;
    output.suppressKey = config.suppressKey;
    return output;
}

Configuration::Config FromApi(const ConfigurationV1& input) {
    Configuration::Config output;
    output.enabled = input.enabled != 0;
    output.virtualJoy = input.virtualJoy != 0;
    output.vJoyToggleAlt = input.vJoyToggleAlt != 0;
    output.enableLog = input.enableLog != 0;
    output.vJoyToggleKey = input.vJoyToggleKey;
    output.radius = input.radius;
    output.idleMs = input.idleMilliseconds;
    output.decayRate = input.decayRate;
    output.pollRateHz = input.pollRateHz;
    output.suppressKey = input.suppressKey;
    return output;
}

Result __cdecl GetStatus(StatusV1* output) noexcept {
    if (!output || output->structSize < sizeof(StatusV1)) {
        return Result::InvalidArgument;
    }
    try {
        const auto config = Configuration::Get();
        output->hookInstalled = SuiteState::HookInstalled() ? 1 : 0;
        output->runtimeActive =
            SuiteState::HookInstalled() && !SuiteState::InactiveByHOTAS() && config.enabled ? 1 : 0;
        output->sourceAvailable = MouseAlignHook::GetSourcePtr() ? 1 : 0;
        output->configurationDirty = Configuration::Dirty() ? 1 : 0;
        output->inactiveByHOTAS = SuiteState::InactiveByHOTAS() ? 1 : 0;
        output->hosted = SuiteState::HostActive() ? 1 : 0;
        output->workbenchOpen = SuiteState::WorkbenchOpen() ? 1 : 0;
        output->inputCaptureActive = SuiteState::InputCaptureActive() ? 1 : 0;
        output->configurationGeneration = Configuration::Generation();
        output->virtualYaw = MouseAlignHook::GetVirtualYaw();
        output->virtualPitch = MouseAlignHook::GetVirtualPitch();
        return SuiteState::InactiveByHOTAS() ? Result::InactiveByHOTAS : Result::Ok;
    } catch (...) {
        return Result::NotReady;
    }
}

Result __cdecl GetConfiguration(ConfigurationV1* output) noexcept {
    if (!output || output->structSize < sizeof(ConfigurationV1)) {
        return Result::InvalidArgument;
    }
    try {
        *output = ToApi(Configuration::Get());
        return Result::Ok;
    } catch (...) {
        return Result::NotReady;
    }
}

Result __cdecl ApplyConfiguration(const ConfigurationV1* input) noexcept {
    if (!input || input->structSize < sizeof(ConfigurationV1) || !std::isfinite(input->radius) ||
        !std::isfinite(input->decayRate)) {
        return Result::InvalidArgument;
    }
    try {
        return Configuration::Apply(FromApi(*input)) ? Result::Ok : Result::InvalidArgument;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl SaveConfiguration() noexcept {
    try {
        return Configuration::Save() ? Result::Ok : Result::WriteFailure;
    } catch (...) {
        return Result::WriteFailure;
    }
}

Result __cdecl ReloadConfiguration() noexcept {
    try {
        Configuration::Load();
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl SetWorkbenchSessionState(std::uint8_t open,
                                        std::uint8_t inputCaptureActive) noexcept {
    SuiteState::SetWorkbenchSession(open != 0, inputCaptureActive != 0);
    return Result::Ok;
}

const ApiV1 g_api{
    .structSize = sizeof(ApiV1),
    .abiVersion = kAbiVersion,
    .moduleId = "absolute.zero",
    .displayName = "AbsoluteZero Ship Control",
    .version = "0.2.0-alpha",
    .getStatus = &GetStatus,
    .getConfiguration = &GetConfiguration,
    .applyConfiguration = &ApplyConfiguration,
    .saveConfiguration = &SaveConfiguration,
    .reloadConfiguration = &ReloadConfiguration,
    .setWorkbenchSessionState = &SetWorkbenchSessionState,
};
} // namespace

extern "C" __declspec(dllexport) const AbsoluteZeroApi::ApiV1*
AbsoluteZero_QueryApi(std::uint32_t requestedAbiVersion) noexcept {
    return requestedAbiVersion == AbsoluteZeroApi::kAbiVersion ? &g_api : nullptr;
}
