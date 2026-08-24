#include "ConfigPolicy.h"

#include <cassert>
#include <limits>

int main() {
    auto config = ConfigPolicy::Defaults();
    config.radius = 900.0F;
    config.idleMs = -4;
    config.decayRate = 0.1F;
    config.pollRateHz = 10'000;
    config.suppressKey = -1;
    config.centerKey = 900;
    config.vJoyToggleKey = 600;
    const auto sanitized = ConfigPolicy::Sanitize(config);
    assert(sanitized.radius == 200.0F);
    assert(sanitized.idleMs == 10);
    assert(sanitized.decayRate == 0.5F);
    assert(sanitized.pollRateHz == 500);
    assert(sanitized.suppressKey == 0);
    assert(sanitized.centerKey == 255);
    assert(sanitized.vJoyToggleKey == 255);

    config.radius = std::numeric_limits<float>::quiet_NaN();
    assert(!ConfigPolicy::HasFiniteNumbers(config));
    assert(ConfigPolicy::Sanitize(config) == ConfigPolicy::Defaults());
}
