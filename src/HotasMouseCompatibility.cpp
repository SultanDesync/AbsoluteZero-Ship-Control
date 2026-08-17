#include "HotasMouseCompatibility.h"

#include "AbsoluteMouseSteeringAPI.h"

#include <windows.h>

#include <cstring>

namespace {

const AbsoluteMouseSteeringApi::ApiV1* g_api{};

bool Valid(const AbsoluteMouseSteeringApi::ApiV1* api) noexcept {
    return api && api->structSize >= sizeof(AbsoluteMouseSteeringApi::ApiV1) &&
           api->abiVersion == AbsoluteMouseSteeringApi::kAbiVersion &&
           api->providerId && std::strcmp(api->providerId, "absolute.hotas") == 0 &&
           api->readMouseAccumulator && api->writeMouseAccumulator &&
           api->rotationalWriterHookInstalled &&
           api->declareAbsoluteZeroOwner &&
           api->absoluteZeroOwnsPitchYaw;
}

} // namespace

namespace HotasMouseCompatibility {

Connection Connect() noexcept {
    const HMODULE module = GetModuleHandleW(L"AbsoluteHOTAS.dll");
    if (!module) {
        return Connection::ModuleNotLoaded;
    }

    using Query = const AbsoluteMouseSteeringApi::ApiV1*(__cdecl*)(std::uint32_t) noexcept;
    const FARPROC address = GetProcAddress(module, "AbsoluteHOTAS_QueryMouseSteeringApi");
    if (!address) {
        return Connection::ApiUnavailable;
    }

    const auto* api = reinterpret_cast<Query>(address)(AbsoluteMouseSteeringApi::kAbiVersion);
    if (!Valid(api)) {
        return Connection::ApiUnavailable;
    }
    if (!api->rotationalWriterHookInstalled()) {
        return Connection::CaptureUnavailable;
    }
    if (api->declareAbsoluteZeroOwner() != AbsoluteMouseSteeringApi::Result::Ok ||
        !api->absoluteZeroOwnsPitchYaw()) {
        return Connection::OwnershipNotAccepted;
    }

    g_api = api;
    return Connection::Connected;
}

bool __cdecl ReadMouseAccumulator(float* yaw, float* pitch) noexcept {
    return g_api && g_api->readMouseAccumulator &&
           g_api->readMouseAccumulator(yaw, pitch) ==
               AbsoluteMouseSteeringApi::Result::Ok;
}

bool __cdecl WriteMouseAccumulator(float yaw, float pitch) noexcept {
    return g_api && g_api->writeMouseAccumulator &&
           g_api->writeMouseAccumulator(yaw, pitch) ==
               AbsoluteMouseSteeringApi::Result::Ok;
}

} // namespace HotasMouseCompatibility
