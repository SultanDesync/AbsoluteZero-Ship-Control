#pragma once

#include "AbsoluteControlPanelAPI.h"

#include <array>
#include <string_view>

namespace ControlPanelSubscriber::Discovery {

inline constexpr std::array<std::wstring_view, 2> kHostModuleNames{
    L"AbsoluteControlPanel.dll",
    L"AbsoluteControlPanelResearchDev.dll",
};

enum class RegistrationDecision {
    UseControlPanel,
    RetryAtPostPostDataLoad,
    UseFallback,
};

[[nodiscard]] constexpr RegistrationDecision DecideRegistration(
    AbsoluteControlPanelApi::Result result, bool finalAttempt) noexcept {
    using AbsoluteControlPanelApi::Result;
    if (result == Result::Ok) {
        return RegistrationDecision::UseControlPanel;
    }
    if (result == Result::NotReady && !finalAttempt) {
        return RegistrationDecision::RetryAtPostPostDataLoad;
    }
    return RegistrationDecision::UseFallback;
}

} // namespace ControlPanelSubscriber::Discovery
