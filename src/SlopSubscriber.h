#pragma once

#include "ConfigPolicy.h"
#include "SlopSDK/SlopAPI.h"

namespace SlopSubscriber {
using QueryApiCallback = const SlopApi::ApiV1*(__cdecl*)(std::uint32_t) noexcept;

// Registers the stable page descriptor with a queried SLOP v1 API.
[[nodiscard]] SlopApi::Result RegisterWith(
    QueryApiCallback query, const SlopApi::PageDescriptorV1& page) noexcept;

struct ConfigurationAccess {
    ConfigPolicy::Config (*get)();
    bool (*apply)(const ConfigPolicy::Config&);
    bool (*save)();
};

// Test seam; production uses Configuration::{Get,Apply,Save}.
void SetConfigurationAccess(ConfigurationAccess access) noexcept;
void ResetForTests() noexcept;
[[nodiscard]] const SlopApi::PageDescriptorV1& Page() noexcept;
[[nodiscard]] const SlopApi::ModuleDescriptorV1& Module() noexcept;
[[nodiscard]] SlopApi::Result RegisterDiscoveredHost() noexcept;
} // namespace SlopSubscriber
