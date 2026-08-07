#pragma once

#include <algorithm>
#include <cmath>
#include <compare>

namespace ConfigPolicy {
struct Config {
    bool enabled{true};
    bool virtualJoy{};
    int vJoyToggleKey{0x60};
    bool vJoyToggleAlt{true};
    float radius{140.0F};
    int idleMs{80};
    float decayRate{12.0F};
    int pollRateHz{120};
    int suppressKey{0x42};
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
    config.vJoyToggleKey = std::clamp(config.vJoyToggleKey, 0, 255);
    return config;
}
} // namespace ConfigPolicy
