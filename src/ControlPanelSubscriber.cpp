#include "ControlPanelSubscriber.h"

#include "Configuration.h"
#include "ControlPanelDiscovery.h"
#include "LiveComponentsExperimentalAPI.h"
#include "SuppressBindingCodec.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string_view>
#include <windows.h>

namespace {
using namespace AbsoluteControlPanelApi;
using Config = ConfigPolicy::Config;

constexpr std::string_view kZeroModuleId = "absolute.zero";

Config ProductionGet() { return Configuration::Get(); }
bool ProductionApply(const Config& config) { return Configuration::Apply(config); }
bool ProductionSave() { return Configuration::Save(); }

ControlPanelSubscriber::Testing::ConfigurationAccess g_access{
    ProductionGet, ProductionApply, ProductionSave };
std::mutex g_editMutex;
Config g_rollback{};
bool g_editing{};
std::mutex g_registrationMutex;
std::atomic<const ApiV1*> g_hostApi{};
std::atomic<bool> g_registered{};
std::atomic<bool> g_livePreviewRegistered{};
std::atomic<bool> g_terminalRejection{};
std::atomic<std::uint64_t> g_liveSequence{};

template <std::size_t N>
void Copy(char (&target)[N], std::string_view source) noexcept {
    const auto count = (std::min)(source.size(), N - 1);
    std::memcpy(target, source.data(), count);
    target[count] = '\0';
}

ValueV1 Boolean(bool value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::Boolean;
    result.booleanValue = value ? 1U : 0U;
    return result;
}

ValueV1 Integer(int value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::Integer;
    result.integerValue = value;
    return result;
}

ValueV1 Float(float value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::Float;
    result.floatValue = value;
    return result;
}

ValueV1 String(std::string_view value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::String;
    Copy(result.stringValue, value);
    return result;
}

ControlDescriptorV1 Control(ControlKind kind, std::uint32_t flags,
    std::string_view id, std::string_view label, std::string_view description,
    double minimum = 0.0, double maximum = 0.0, double step = 0.0) {
    ControlDescriptorV1 control;
    control.kind = kind;
    control.flags = flags;
    Copy(control.controlId, id);
    Copy(control.label, label);
    Copy(control.description, description);
    control.minimumValue = minimum;
    control.maximumValue = maximum;
    control.stepValue = step;
    return control;
}

const std::array g_controls{
    Control(ControlKind::Toggle, kControlNone, "enabled", "Enable auto-centering",
        "Gently return native ship mouse steering toward center after input stops."),
    Control(ControlKind::Choice, kControlNone, "activation-mode", "Activation mode",
        "Choose automatic idle centering or a one-shot center command.", 0, 1, 1),
    Control(ControlKind::FloatSlider, kControlNone, "radius", "Activation radius",
        "Only center the native mouse accumulator while it remains inside this radius.",
        1.0, 200.0, 1.0),
    Control(ControlKind::IntegerSlider, kControlNone, "idle-ms", "Idle delay",
        "Milliseconds without mouse movement before centering begins.", 10, 500, 10),
    Control(ControlKind::FloatSlider, kControlNone, "decay-rate", "Centering rate",
        "How quickly steering returns toward center; higher values settle faster.",
        0.5, 20.0, 0.5),
    Control(ControlKind::InputBinding, kBindingKeyboard | kBindingClearable,
        "suppress-binding", "Hold to suppress",
        "In Automatic mode, temporarily suspend centering while this key is held."),
    Control(ControlKind::InputBinding, kBindingKeyboard | kBindingClearable,
        "center-binding", "Center on press",
        "In Center on press mode, press this key once to begin centering immediately."),
    Control(ControlKind::IntegerSlider, kControlAdvanced, "poll-rate-hz", "Polling rate",
        "Alignment worker polling frequency in hertz.", 30, 500, 10),
    Control(ControlKind::Toggle, kControlAdvanced | kControlRequiresRestart,
        "diagnostic-log", "Diagnostic logging",
        "Write detailed AbsoluteZero diagnostics after restart.")
};

Result __cdecl Read(void*, const char* rawId, ValueV1* output) noexcept {
    if (!rawId || !output || output->structSize < sizeof(ValueV1)) {
        return Result::InvalidArgument;
    }
    try {
        const std::string_view id{ rawId };
        const Config config = g_access.get();
        if (id == "enabled") *output = Boolean(config.enabled);
        else if (id == "activation-mode") *output = Integer(config.centerOnPress ? 1 : 0);
        else if (id == "radius") *output = Float(config.radius);
        else if (id == "idle-ms") *output = Integer(config.idleMs);
        else if (id == "decay-rate") *output = Float(config.decayRate);
        else if (id == "suppress-binding") {
            *output = String(SuppressBindingCodec::Display(config.suppressKey));
        } else if (id == "center-binding") {
            *output = String(SuppressBindingCodec::Display(config.centerKey));
        } else if (id == "poll-rate-hz") *output = Integer(config.pollRateHz);
        else if (id == "diagnostic-log") *output = Boolean(config.enableLog);
        else return Result::NotFound;
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

bool BooleanValue(const ValueV1& value, bool& output) noexcept {
    if (value.kind != ValueKind::Boolean || value.booleanValue > 1) return false;
    output = value.booleanValue != 0;
    return true;
}

bool IntegerValue(const ValueV1& value, int minimum, int maximum,
    int step, int& output) noexcept {
    if (value.kind != ValueKind::Integer || value.integerValue < minimum ||
        value.integerValue > maximum || (value.integerValue - minimum) % step != 0) {
        return false;
    }
    output = static_cast<int>(value.integerValue);
    return true;
}

bool FloatValue(const ValueV1& value, float minimum, float maximum,
    float& output) noexcept {
    if (value.kind != ValueKind::Float || !std::isfinite(value.floatValue) ||
        value.floatValue < minimum || value.floatValue > maximum) {
        return false;
    }
    output = static_cast<float>(value.floatValue);
    return true;
}

Result __cdecl Write(void*, const char* rawId, const ValueV1* value) noexcept {
    if (!rawId || !value || value->structSize < sizeof(ValueV1)) {
        return Result::InvalidArgument;
    }
    try {
        std::lock_guard lock(g_editMutex);
        const std::string_view id{ rawId };
        const Config current = g_access.get();
        Config proposed = current;
        bool accepted{};
        if (id == "enabled") {
            accepted = BooleanValue(*value, proposed.enabled);
        } else if (id == "activation-mode") {
            int mode{};
            accepted = IntegerValue(*value, 0, 1, 1, mode);
            if (accepted) proposed.centerOnPress = mode == 1;
        } else if (id == "radius") {
            accepted = FloatValue(*value, 1.0F, 200.0F, proposed.radius);
        } else if (id == "idle-ms") {
            accepted = IntegerValue(*value, 10, 500, 10, proposed.idleMs);
        } else if (id == "decay-rate") {
            accepted = FloatValue(*value, 0.5F, 20.0F, proposed.decayRate);
        } else if (id == "suppress-binding") {
            accepted = value->kind == ValueKind::String &&
                std::memchr(value->stringValue, '\0', kStringValueCapacity) &&
                SuppressBindingCodec::Parse(value->stringValue, proposed.suppressKey);
        } else if (id == "center-binding") {
            accepted = value->kind == ValueKind::String &&
                std::memchr(value->stringValue, '\0', kStringValueCapacity) &&
                SuppressBindingCodec::Parse(value->stringValue, proposed.centerKey);
        } else if (id == "poll-rate-hz") {
            accepted = IntegerValue(*value, 30, 500, 10, proposed.pollRateHz);
        } else if (id == "diagnostic-log") {
            accepted = BooleanValue(*value, proposed.enableLog);
        } else {
            return Result::NotFound;
        }
        if (!accepted) return Result::InvalidArgument;
        if (!g_editing) g_rollback = current;
        if (!g_access.apply(proposed)) return Result::WriteFailure;
        g_editing = true;
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl Apply(void*) noexcept {
    try {
        std::lock_guard lock(g_editMutex);
        if (!g_access.save()) return Result::WriteFailure;
        g_editing = false;
        return Result::Ok;
    } catch (...) {
        return Result::WriteFailure;
    }
}

void __cdecl Cancel(void*) noexcept {
    try {
        std::lock_guard lock(g_editMutex);
        if (g_editing) (void)g_access.apply(g_rollback);
        g_editing = false;
    } catch (...) {
    }
}

Result __cdecl ReadChoiceOptions(void*, const char* rawId,
    ChoiceOptionV1* options, std::uint32_t capacity,
    std::uint32_t* count) noexcept {
    if (!rawId || !count) return Result::InvalidArgument;
    if (std::string_view{ rawId } != "activation-mode") return Result::NotFound;
    constexpr std::array<std::pair<std::int64_t, std::string_view>, 2> values{{
        { 0, "Automatic after idle" },
        { 1, "Center on press" },
    }};
    *count = static_cast<std::uint32_t>(values.size());
    if (!options || capacity < values.size()) return Result::CapacityExceeded;
    for (std::size_t index = 0; index < values.size(); ++index) {
        options[index] = {};
        options[index].value = values[index].first;
        Copy(options[index].label, values[index].second);
    }
    return Result::Ok;
}

const ModuleDescriptorV1 g_module = [] {
    ModuleDescriptorV1 module;
    Copy(module.moduleId, kZeroModuleId);
    Copy(module.displayName, "AbsoluteZero");
    Copy(module.description,
        "Idle mouse centering; when paired with HOTAS, native mouse owns pitch and yaw.");
    return module;
}();

const PageDescriptorV1 g_page = [] {
    PageDescriptorV1 page;
    Copy(page.moduleId, kZeroModuleId);
    Copy(page.pageId, "mouse-alignment");
    Copy(page.displayName, "Mouse Alignment");
    Copy(page.description,
        "Tune when native mouse steering recenters after input stops.");
    page.controlCount = static_cast<std::uint32_t>(g_controls.size());
    page.controls = g_controls.data();
    page.readValue = &Read;
    page.writeDraft = &Write;
    page.apply = &Apply;
    page.cancel = &Cancel;
    page.readChoiceOptions = &ReadChoiceOptions;
    return page;
}();

namespace Live = AbsoluteControlPanelExperimental;

Live::Result __cdecl ReadCenteringPreview(
    void*, Live::LiveFrameV1* output) noexcept {
    if (!output || output->structSize < sizeof(Live::LiveFrameV1)) {
        return Live::Result::InvalidArgument;
    }
    Live::LiveFrameV1 frame;
    frame.kind = Live::ComponentKind::RadialResponse;
    frame.sequence = g_liveSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    frame.monotonicTimestampUs = (std::max)(1ULL, GetTickCount64() * 1000ULL);
    frame.radialResponse.available = 1;
    frame.radialResponse.liveX = 0.0;
    frame.radialResponse.liveY = 0.0;
    *output = frame;
    return Live::Result::Ok;
}

const Live::LiveChannelDescriptorV1 g_livePreview = [] {
    Live::LiveChannelDescriptorV1 descriptor;
    Copy(descriptor.moduleId, kZeroModuleId);
    Copy(descriptor.pageId, "mouse-alignment");
    Copy(descriptor.channelId, "centering-lab");
    Copy(descriptor.title, "Mouse centering response");
    descriptor.kind = Live::ComponentKind::RadialResponse;
    descriptor.flags = Live::kLivePresentationPinned;
    descriptor.radialResponse.maximumRadius = 200.0;
    Copy(descriptor.radialResponse.enabledControlId, "enabled");
    Copy(descriptor.radialResponse.radiusControlId, "radius");
    Copy(descriptor.radialResponse.idleMillisecondsControlId, "idle-ms");
    Copy(descriptor.radialResponse.decayRateControlId, "decay-rate");
    Copy(descriptor.radialResponse.pollRateControlId, "poll-rate-hz");
    descriptor.readLiveFrame = &ReadCenteringPreview;
    return descriptor;
}();

bool ValidApi(const ApiV1* api) noexcept {
    constexpr auto required = offsetof(ApiV1, registerModule) +
        sizeof(api->registerModule);
    return api && api->structSize >= required && api->abiVersion == kAbiVersion &&
        api->moduleId && std::string_view(api->moduleId) == kModuleId &&
        api->registerPage && api->unregisterModule && api->requestRefresh &&
        api->registerModule;
}

const ApiV1* __cdecl ResolveLoadedHost(const wchar_t* moduleName) noexcept {
    if (!moduleName) return nullptr;
    const auto module = GetModuleHandleW(moduleName);
    if (!module) return nullptr;
    const auto address = GetProcAddress(module, "AbsoluteControlPanel_QueryApi");
    if (!address) return nullptr;
    const auto query = reinterpret_cast<AbsoluteControlPanelQueryApi>(address);
    return query(kAbiVersion);
}

const Live::ExperimentalApiV1* ResolveLoadedLiveHost(
    const wchar_t* moduleName) noexcept {
    if (!moduleName) return nullptr;
    const auto module = GetModuleHandleW(moduleName);
    if (!module) return nullptr;
    const auto address = GetProcAddress(module,
        "AbsoluteControlPanel_QueryLiveComponentsExperimental");
    if (!address) return nullptr;
    using Query = const Live::ExperimentalApiV1*(__cdecl*)(
        std::uint32_t) noexcept;
    return reinterpret_cast<Query>(address)(Live::kAbiVersion);
}

Result RegistrationFailure(Result result) noexcept {
    if (result == Result::NotReady) return result;
    g_terminalRejection.store(true, std::memory_order_release);
    return result;
}
} // namespace

namespace ControlPanelSubscriber {

AbsoluteControlPanelApi::Result RegisterDiscoveredHost() noexcept {
    const auto result = Testing::RegisterWithResolver(&ResolveLoadedHost);
    if (result == Result::Ok &&
        !g_livePreviewRegistered.load(std::memory_order_acquire)) {
        for (const auto moduleName : Discovery::kHostModuleNames) {
            if (const auto* live = ResolveLoadedLiveHost(moduleName.data())) {
                (void)Testing::RegisterLivePreview(live);
                break;
            }
        }
    }
    return result;
}

bool IsHosted() noexcept {
    return g_registered.load(std::memory_order_acquire);
}

bool IsMenuOpen() noexcept {
    try {
        const auto* api = g_hostApi.load(std::memory_order_acquire);
        constexpr auto required = offsetof(ApiV1, isOpen) + sizeof(api->isOpen);
        return g_registered.load(std::memory_order_acquire) && api &&
            api->structSize >= required && api->isOpen && api->isOpen() != 0;
    } catch (...) {
        return false;
    }
}

bool IsInputCaptureActive() noexcept {
    try {
        const auto* api = g_hostApi.load(std::memory_order_acquire);
        constexpr auto required = offsetof(ApiV1, isInputCaptureActive) +
            sizeof(api->isInputCaptureActive);
        return g_registered.load(std::memory_order_acquire) && api &&
            api->structSize >= required && api->isInputCaptureActive &&
            api->isInputCaptureActive() != 0;
    } catch (...) {
        return false;
    }
}

namespace Testing {

AbsoluteControlPanelApi::Result RegisterWithResolver(
    ResolveLoadedHostCallback resolver) noexcept {
    if (!resolver) return Result::InvalidArgument;
    if (g_registered.load(std::memory_order_acquire)) return Result::Ok;
    if (g_terminalRejection.load(std::memory_order_acquire)) return Result::Rejected;
    try {
        std::scoped_lock lock(g_registrationMutex);
        if (g_registered.load(std::memory_order_relaxed)) return Result::Ok;
        if (g_terminalRejection.load(std::memory_order_relaxed)) return Result::Rejected;

        for (const auto moduleName : Discovery::kHostModuleNames) {
            const auto* api = resolver(moduleName.data());
            if (!api) continue;
            if (!ValidApi(api)) {
                g_terminalRejection.store(true, std::memory_order_release);
                return Result::Rejected;
            }
            const auto moduleResult = api->registerModule(&g_module);
            if (moduleResult != Result::Ok && moduleResult != Result::Duplicate) {
                return RegistrationFailure(moduleResult);
            }
            const bool ownsModule = moduleResult == Result::Ok;
            const auto pageResult = api->registerPage(&g_page);
            if (pageResult != Result::Ok && pageResult != Result::Duplicate) {
                if (ownsModule) (void)api->unregisterModule(kZeroModuleId.data());
                return RegistrationFailure(pageResult);
            }
            g_hostApi.store(api, std::memory_order_release);
            g_registered.store(true, std::memory_order_release);
            return Result::Ok;
        }
        return Result::NotFound;
    } catch (...) {
        return Result::Rejected;
    }
}

void SetConfigurationAccess(ConfigurationAccess access) noexcept {
    std::lock_guard lock(g_editMutex);
    g_access = access.get && access.apply && access.save ? access :
        ConfigurationAccess{ ProductionGet, ProductionApply, ProductionSave };
}

AbsoluteControlPanelApi::Result RegisterLivePreview(
    const AbsoluteControlPanelExperimental::ExperimentalApiV1* api) noexcept {
    if (!api || api->structSize < sizeof(Live::ExperimentalApiV1) ||
        api->abiVersion != Live::kAbiVersion || !api->registerLiveChannel ||
        (api->capabilities & Live::kLiveCapabilityRadialResponse) == 0) {
        return Result::InvalidArgument;
    }
    const auto result = api->registerLiveChannel(&g_livePreview);
    if (result == Live::Result::Ok || result == Live::Result::Duplicate) {
        g_livePreviewRegistered.store(true, std::memory_order_release);
        return Result::Ok;
    }
    return static_cast<Result>(result);
}

void Reset() noexcept {
    {
        std::lock_guard lock(g_editMutex);
        g_access = { ProductionGet, ProductionApply, ProductionSave };
        g_rollback = {};
        g_editing = false;
    }
    g_hostApi.store(nullptr, std::memory_order_release);
    g_registered.store(false, std::memory_order_release);
    g_livePreviewRegistered.store(false, std::memory_order_release);
    g_liveSequence.store(0, std::memory_order_release);
    g_terminalRejection.store(false, std::memory_order_release);
}

const PageDescriptorV1& Page() noexcept { return g_page; }
const ModuleDescriptorV1& Module() noexcept { return g_module; }
const AbsoluteControlPanelExperimental::LiveChannelDescriptorV1&
LivePreview() noexcept { return g_livePreview; }

} // namespace Testing
} // namespace ControlPanelSubscriber
