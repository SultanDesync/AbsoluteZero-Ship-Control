#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace AbsoluteZeroApi {
inline constexpr std::uint32_t kAbiVersion = 1;

enum class Result : std::uint32_t {
    Ok,
    NotReady,
    InactiveByHOTAS,
    InvalidArgument,
    WriteFailure,
    Rejected,
};

struct ConfigurationV1 {
    std::uint32_t structSize{sizeof(ConfigurationV1)};
    std::uint8_t enabled{};
    std::uint8_t virtualJoy{};
    std::uint8_t vJoyToggleAlt{};
    std::uint8_t enableLog{};
    std::int32_t vJoyToggleKey{};
    float radius{};
    std::int32_t idleMilliseconds{};
    float decayRate{};
    std::int32_t pollRateHz{};
    std::int32_t suppressKey{};
};

struct StatusV1 {
    std::uint32_t structSize{sizeof(StatusV1)};
    std::uint8_t hookInstalled{};
    std::uint8_t runtimeActive{};
    std::uint8_t sourceAvailable{};
    std::uint8_t configurationDirty{};
    std::uint8_t inactiveByHOTAS{};
    std::uint8_t hosted{};
    std::uint8_t workbenchOpen{};
    std::uint8_t inputCaptureActive{};
    std::uint64_t configurationGeneration{};
    float virtualYaw{};
    float virtualPitch{};
};

struct ApiV1 {
    std::uint32_t structSize{sizeof(ApiV1)};
    std::uint32_t abiVersion{kAbiVersion};
    const char* moduleId{};
    const char* displayName{};
    const char* version{};

    Result(__cdecl* getStatus)(StatusV1*) noexcept{};
    Result(__cdecl* getConfiguration)(ConfigurationV1*) noexcept{};
    Result(__cdecl* applyConfiguration)(const ConfigurationV1*) noexcept{};
    Result(__cdecl* saveConfiguration)() noexcept{};
    Result(__cdecl* reloadConfiguration)() noexcept{};
    Result(__cdecl* setWorkbenchSessionState)(std::uint8_t open,
                                              std::uint8_t inputCaptureActive) noexcept{};
};

static_assert(std::is_standard_layout_v<ApiV1>);
static_assert(std::is_trivially_copyable_v<ConfigurationV1>);
static_assert(std::is_trivially_copyable_v<StatusV1>);
} // namespace AbsoluteZeroApi

#if defined(ABSOLUTE_ZERO_EXPORTS)
#define ABSOLUTE_ZERO_API __declspec(dllexport)
#else
#define ABSOLUTE_ZERO_API __declspec(dllimport)
#endif
extern "C" ABSOLUTE_ZERO_API const AbsoluteZeroApi::ApiV1*
AbsoluteZero_QueryApi(std::uint32_t requestedAbiVersion) noexcept;
