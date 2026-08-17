#pragma once

#include <cstdint>

namespace HotasMouseCompatibility {

enum class Connection {
    Connected,
    ModuleNotLoaded,
    ApiUnavailable,
    CaptureUnavailable,
    OwnershipNotAccepted,
};

[[nodiscard]] Connection Connect() noexcept;
[[nodiscard]] bool __cdecl ReadMouseAccumulator(float* yaw, float* pitch) noexcept;
[[nodiscard]] bool __cdecl WriteMouseAccumulator(float yaw, float pitch) noexcept;

} // namespace HotasMouseCompatibility
