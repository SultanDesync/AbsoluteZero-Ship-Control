#include "AbsoluteZeroAPI.h"

#include <cassert>
#include <type_traits>

int main() {
    using namespace AbsoluteZeroApi;
    static_assert(std::is_standard_layout_v<ApiV1>);
    static_assert(std::is_trivially_copyable_v<ConfigurationV1>);
    static_assert(std::is_trivially_copyable_v<StatusV1>);
    assert(ConfigurationV1{}.structSize == sizeof(ConfigurationV1));
    assert(StatusV1{}.structSize == sizeof(StatusV1));
    assert(ApiV1{}.abiVersion == kAbiVersion);
}
