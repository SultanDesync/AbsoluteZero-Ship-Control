#include "HotasMouseCompatibility.h"

#include "AbsoluteMouseSteeringAPI.h"

#include <windows.h>

#include <cstring>

namespace {

const AbsoluteMouseSteeringApi::ApiV1* g_api{};
const AbsoluteMouseSteeringApi::ApiV2* g_apiV2{};

bool ValidV1(const AbsoluteMouseSteeringApi::ApiV1* api) noexcept {
    return api && api->structSize >= sizeof(AbsoluteMouseSteeringApi::ApiV1) &&
           (api->abiVersion == AbsoluteMouseSteeringApi::kAbiVersion1 ||
            api->abiVersion == AbsoluteMouseSteeringApi::kAbiVersion) &&
           api->providerId && std::strcmp(api->providerId, "absolute.hotas") == 0 &&
           api->readMouseAccumulator && api->writeMouseAccumulator &&
           api->rotationalWriterHookInstalled &&
           api->declareAbsoluteZeroOwner &&
           api->absoluteZeroOwnsPitchYaw;
}

bool ValidV2(const AbsoluteMouseSteeringApi::ApiV2* api) noexcept {
    return api && api->structSize >= sizeof(AbsoluteMouseSteeringApi::ApiV2) &&
           api->abiVersion == AbsoluteMouseSteeringApi::kAbiVersion &&
           api->providerId && std::strcmp(api->providerId, "absolute.hotas") == 0 &&
           api->readMouseAccumulator && api->writeMouseAccumulator &&
           api->rotationalWriterHookInstalled &&
           api->declareAbsoluteZeroOwner &&
           api->absoluteZeroOwnsPitchYaw &&
           api->extractMouseAccumulatorDelta &&
           api->setVirtualJoySteering &&
           api->registerWriterSyncCallback;
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

    const auto query = reinterpret_cast<Query>(address);
    // First try querying V2
    const auto* apiV2Candidate = reinterpret_cast<const AbsoluteMouseSteeringApi::ApiV2*>(
        query(AbsoluteMouseSteeringApi::kAbiVersion));

    const AbsoluteMouseSteeringApi::ApiV1* api = nullptr;
    if (ValidV2(apiV2Candidate)) {
        g_apiV2 = apiV2Candidate;
        api = reinterpret_cast<const AbsoluteMouseSteeringApi::ApiV1*>(apiV2Candidate);
    } else {
        // Fall back to querying V1
        api = query(AbsoluteMouseSteeringApi::kAbiVersion1);
        if (!ValidV1(api)) {
            return Connection::ApiUnavailable;
        }
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

bool SupportsVirtualJoy() noexcept {
    return g_apiV2 != nullptr;
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

bool ExtractMouseAccumulatorDelta(float* yaw, float* pitch, bool zeroAccumulator) noexcept {
    return g_apiV2 && g_apiV2->extractMouseAccumulatorDelta &&
           g_apiV2->extractMouseAccumulatorDelta(yaw, pitch, zeroAccumulator ? 1 : 0) ==
               AbsoluteMouseSteeringApi::Result::Ok;
}

bool SetVirtualJoySteering(bool enabled, float yawNorm, float pitchNorm) noexcept {
    return g_apiV2 && g_apiV2->setVirtualJoySteering &&
           g_apiV2->setVirtualJoySteering(enabled ? 1 : 0, yawNorm, pitchNorm) ==
               AbsoluteMouseSteeringApi::Result::Ok;
}

bool RegisterWriterSyncCallback(void(__cdecl* callback)() noexcept) noexcept {
    return g_apiV2 && g_apiV2->registerWriterSyncCallback &&
           g_apiV2->registerWriterSyncCallback(callback) ==
               AbsoluteMouseSteeringApi::Result::Ok;
}

} // namespace HotasMouseCompatibility
