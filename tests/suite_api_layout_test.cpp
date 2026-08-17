#include "AbsoluteZeroAPI.h"
#include "AbsoluteMouseSteeringAPI.h"

#include <cassert>
#include <type_traits>

int main() {
    using namespace AbsoluteZeroApi;
    static_assert(std::is_standard_layout_v<ApiV1>);
    static_assert(std::is_trivially_copyable_v<ConfigurationV1>);
    static_assert(std::is_trivially_copyable_v<StatusV1>);
    assert(ConfigurationV1{}.structSize == sizeof(ConfigurationV1));
    assert(StatusV1{}.structSize == sizeof(StatusV1));
    static_assert(sizeof(AbsoluteMouseSteeringApi::ApiV1) == 56);
    static_assert(offsetof(AbsoluteMouseSteeringApi::ApiV1, readMouseAccumulator) == 16);
    static_assert(offsetof(AbsoluteMouseSteeringApi::ApiV1, declareAbsoluteZeroOwner) == 40);
    static_assert(offsetof(AbsoluteMouseSteeringApi::ApiV1, absoluteZeroOwnsPitchYaw) == 48);
    assert(ApiV1{}.abiVersion == kAbiVersion);
}
