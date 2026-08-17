#pragma once

#include "AbsoluteControlPanelAPI.h"
#include "ConfigPolicy.h"

namespace ControlPanelSubscriber {

[[nodiscard]] AbsoluteControlPanelApi::Result RegisterDiscoveredHost() noexcept;
[[nodiscard]] bool IsHosted() noexcept;
[[nodiscard]] bool IsMenuOpen() noexcept;
[[nodiscard]] bool IsInputCaptureActive() noexcept;

namespace Testing {

using ResolveLoadedHostCallback = const AbsoluteControlPanelApi::ApiV1*(__cdecl*)(
    const wchar_t* moduleName) noexcept;

struct ConfigurationAccess {
    ConfigPolicy::Config (*get)();
    bool (*apply)(const ConfigPolicy::Config&);
    bool (*save)();
};

[[nodiscard]] AbsoluteControlPanelApi::Result RegisterWithResolver(
    ResolveLoadedHostCallback resolver) noexcept;
void SetConfigurationAccess(ConfigurationAccess access) noexcept;
void Reset() noexcept;
[[nodiscard]] const AbsoluteControlPanelApi::PageDescriptorV1& Page() noexcept;
[[nodiscard]] const AbsoluteControlPanelApi::ModuleDescriptorV1& Module() noexcept;

} // namespace Testing
} // namespace ControlPanelSubscriber
