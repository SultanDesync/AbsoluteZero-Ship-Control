#pragma once

#include "ConfigPolicy.h"

#include <cstdint>

namespace Configuration {
using Config = ConfigPolicy::Config;

void Load();
[[nodiscard]] Config Get();
[[nodiscard]] bool Apply(const Config& config);
[[nodiscard]] bool Save();
[[nodiscard]] bool Dirty();
[[nodiscard]] std::uint64_t Generation();
} // namespace Configuration
