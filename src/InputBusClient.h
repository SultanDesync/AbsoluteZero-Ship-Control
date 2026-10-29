#pragma once

#include "AbsoluteControlPanelAPI.h"
#include "AbsoluteInputBusAPI.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct JoystickBinding {
    std::string persistentId; // e.g. "{GUID}"
    std::uint32_t channelIndex{}; // 0..127 = button 1..128, 128..143 = POV 129..144
    std::string originalToken;

    bool operator==(const JoystickBinding&) const = default;
};

namespace JoystickBindingPolicy {
[[nodiscard]] bool ValidToken(std::string_view token) noexcept;
[[nodiscard]] std::optional<JoystickBinding> Parse(std::string_view token) noexcept;
[[nodiscard]] std::string FormatDisplay(std::string_view token,
                                        std::string_view deviceProductName = {}) noexcept;
} // namespace JoystickBindingPolicy

class InputBusClient {
public:
    static InputBusClient& Get() noexcept;

    // Discover AbsoluteHOTAS_QueryInputBusApi(1) dynamically.
    bool Discover() noexcept;

    [[nodiscard]] bool IsAvailable() const noexcept;

    // Binding capture bridge for Absolute Control Panel
    AbsoluteControlPanelApi::Result BeginCapture(const char* controlId) noexcept;
    AbsoluteControlPanelApi::Result PollCapture(
        const char* controlId,
        AbsoluteControlPanelApi::BindingCaptureV1* output) noexcept;
    AbsoluteControlPanelApi::Result CancelCapture(const char* controlId) noexcept;

    // DirectInput runtime polling
    [[nodiscard]] bool IsControlDown(std::string_view token) noexcept;
    [[nodiscard]] bool WasControlPressed(std::string_view token) noexcept;

    [[nodiscard]] std::string GetDeviceProductName(std::string_view persistentId) noexcept;
    [[nodiscard]] std::string FormatBinding(std::string_view token) noexcept;

private:
    InputBusClient() = default;

    struct DeviceTrackingState {
        std::uint64_t producerGeneration{};
        std::array<std::uint32_t, AbsoluteInputBusApi::kDigitalControlCount> pressCountBaseline{};
        bool initialized{};
    };

    std::atomic<const AbsoluteInputBusApi::ApiV1*> api_{};
    std::mutex captureMutex_;
    std::uint64_t activeSessionId_{};
    std::string activeControlId_;
    std::array<bool, 256> keyBaseline_{};
    bool captureBaselineTaken_{};

    std::mutex trackingMutex_;
    std::unordered_map<std::string, DeviceTrackingState> trackingByPersistentId_;
};
