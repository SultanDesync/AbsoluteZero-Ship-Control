#pragma once

#include <algorithm>
#include <cmath>
#include <compare>
#include <string>

namespace ConfigPolicy {
struct Config {
    bool enabled{true};
    bool virtualJoy{true};
    int vJoyToggleKey{0x60};
    bool vJoyToggleControl{};
    bool vJoyToggleAlt{true};
    bool vJoyToggleShift{};
    std::string vJoyToggleBinding{"keyboard:0x60;ctrl=0;alt=1;shift=0"};
    float radius{140.0F};
    int idleMs{80};
    float decayRate{12.0F};
    int pollRateHz{120};
    int suppressKey{0x42};
    bool suppressControl{};
    bool suppressAlt{};
    bool suppressShift{};
    std::string suppressBinding{"keyboard:0x42;ctrl=0;alt=0;shift=0"};
    bool centerOnPress{};
    int centerKey{0x43};
    bool centerControl{};
    bool centerAlt{};
    bool centerShift{};
    std::string centerBinding{"keyboard:0x43;ctrl=0;alt=0;shift=0"};
    bool enableLog{};

    auto operator<=>(const Config&) const = default;
};

[[nodiscard]] inline Config Defaults() noexcept { return {}; }

[[nodiscard]] inline bool HasFiniteNumbers(const Config& config) noexcept {
    return std::isfinite(config.radius) && std::isfinite(config.decayRate);
}

[[nodiscard]] inline Config Sanitize(Config config) noexcept {
    if (!HasFiniteNumbers(config)) {
        return Defaults();
    }
    config.radius = std::clamp(config.radius, 1.0F, 200.0F);
    config.idleMs = std::clamp(config.idleMs, 10, 500);
    config.decayRate = std::clamp(config.decayRate, 0.5F, 20.0F);
    config.pollRateHz = std::clamp(config.pollRateHz, 30, 500);
    config.suppressKey = std::clamp(config.suppressKey, 0, 255);
    config.centerKey = std::clamp(config.centerKey, 0, 255);
    config.vJoyToggleKey = std::clamp(config.vJoyToggleKey, 0, 255);
    return config;
}
} // namespace ConfigPolicy
