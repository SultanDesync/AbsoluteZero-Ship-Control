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
[[nodiscard]] bool SupportsVirtualJoy() noexcept;
[[nodiscard]] bool __cdecl ReadMouseAccumulator(float* yaw, float* pitch) noexcept;
[[nodiscard]] bool __cdecl WriteMouseAccumulator(float yaw, float pitch) noexcept;
[[nodiscard]] bool ExtractMouseAccumulatorDelta(float* yaw, float* pitch, bool zeroAccumulator) noexcept;
bool SetVirtualJoySteering(bool enabled, float yawNorm, float pitchNorm) noexcept;
bool RegisterWriterSyncCallback(void(__cdecl* callback)() noexcept) noexcept;

} // namespace HotasMouseCompatibility
