#include "SlopSubscriber.h"

#include "Configuration.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <windows.h>

namespace {
using Config = Configuration::Config;

Config ProductionGet() noexcept { return Configuration::Get(); }
bool ProductionApply(const Config& config) noexcept { return Configuration::Apply(config); }
bool ProductionSave() noexcept { return Configuration::Save(); }
SlopSubscriber::ConfigurationAccess g_access{ ProductionGet, ProductionApply, ProductionSave };
Config g_rollback{};
bool g_editing{};

template <std::size_t N>
void Copy(char (&target)[N], std::string_view source) noexcept {
    const auto count = (std::min)(source.size(), N - 1);
    std::memcpy(target, source.data(), count);
}

SlopApi::ValueV1 Boolean(bool value) noexcept {
    SlopApi::ValueV1 result{};
    result.kind = SlopApi::ValueKind::Boolean;
    result.booleanValue = value ? 1U : 0U;
    return result;
}
SlopApi::ValueV1 Integer(int value) noexcept {
    SlopApi::ValueV1 result{};
    result.kind = SlopApi::ValueKind::Integer;
    result.integerValue = value;
    return result;
}
SlopApi::ValueV1 Float(float value) noexcept {
    SlopApi::ValueV1 result{};
    result.kind = SlopApi::ValueKind::Float;
    result.floatValue = value;
    return result;
}

bool ValidApi(const SlopApi::ApiV1* api) noexcept {
    constexpr std::size_t minimum = offsetof(SlopApi::ApiV1, requestRefresh) +
                                    sizeof(((SlopApi::ApiV1*)nullptr)->requestRefresh);
    return api && api->structSize >= minimum && api->abiVersion == SlopApi::kAbiVersion &&
           api->moduleId && std::string_view(api->moduleId) == "SLOP" && api->registerPage &&
           api->unregisterModule && api->requestRefresh;
}

SlopApi::Result Read(void*, const char* id, SlopApi::ValueV1* value) noexcept {
    if (!id || !value) return SlopApi::Result::InvalidArgument;
    const auto config = g_access.get();
    if (std::strcmp(id, "enabled") == 0) *value = Boolean(config.enabled);
    else if (std::strcmp(id, "radius") == 0) *value = Float(config.radius);
    else if (std::strcmp(id, "idle-ms") == 0) *value = Integer(config.idleMs);
    else if (std::strcmp(id, "decay-rate") == 0) *value = Float(config.decayRate);
    else if (std::strcmp(id, "poll-rate-hz") == 0) *value = Integer(config.pollRateHz);
    else if (std::strcmp(id, "suppress-key") == 0) *value = Integer(config.suppressKey);
    else if (std::strcmp(id, "diagnostic-log") == 0) *value = Boolean(config.enableLog);
    else return SlopApi::Result::NotFound;
    return SlopApi::Result::Ok;
}

SlopApi::Result Write(void*, const char* id, const SlopApi::ValueV1* value) noexcept {
    if (!id || !value || value->structSize < sizeof(SlopApi::ValueV1)) return SlopApi::Result::InvalidArgument;
    const auto current = g_access.get();
    Config proposed = current;
    const auto boolean = [&]() { return value->kind == SlopApi::ValueKind::Boolean && value->booleanValue <= 1; };
    const auto integer = [&]() { return value->kind == SlopApi::ValueKind::Integer; };
    const auto floating = [&]() { return value->kind == SlopApi::ValueKind::Float && std::isfinite(value->floatValue); };
    if (std::strcmp(id, "enabled") == 0 && boolean()) proposed.enabled = value->booleanValue != 0;
    else if (std::strcmp(id, "radius") == 0 && floating() && value->floatValue >= 1 && value->floatValue <= 200) proposed.radius = static_cast<float>(value->floatValue);
    else if (std::strcmp(id, "idle-ms") == 0 && integer() && value->integerValue >= 10 && value->integerValue <= 500 && value->integerValue % 10 == 0) proposed.idleMs = static_cast<int>(value->integerValue);
    else if (std::strcmp(id, "decay-rate") == 0 && floating() && value->floatValue >= .5 && value->floatValue <= 20) proposed.decayRate = static_cast<float>(value->floatValue);
    else if (std::strcmp(id, "poll-rate-hz") == 0 && integer() && value->integerValue >= 30 && value->integerValue <= 500 && value->integerValue % 10 == 0) proposed.pollRateHz = static_cast<int>(value->integerValue);
    else if (std::strcmp(id, "suppress-key") == 0 && integer() && value->integerValue >= 0 && value->integerValue <= 255) proposed.suppressKey = static_cast<int>(value->integerValue);
    else if (std::strcmp(id, "diagnostic-log") == 0 && boolean()) proposed.enableLog = value->booleanValue != 0;
    else return std::strcmp(id, "enabled") && std::strcmp(id, "radius") && std::strcmp(id, "idle-ms") && std::strcmp(id, "decay-rate") && std::strcmp(id, "poll-rate-hz") && std::strcmp(id, "suppress-key") && std::strcmp(id, "diagnostic-log") ? SlopApi::Result::NotFound : SlopApi::Result::InvalidArgument;
    if (!g_editing) { g_rollback = current; g_editing = true; }
    return g_access.apply(proposed) ? SlopApi::Result::Ok : SlopApi::Result::WriteFailure;
}

SlopApi::Result Apply(void*) noexcept { if (!g_access.save()) return SlopApi::Result::WriteFailure; g_editing = false; return SlopApi::Result::Ok; }
void Cancel(void*) noexcept { if (g_editing) (void)g_access.apply(g_rollback); g_editing = false; }

SlopApi::ControlDescriptorV1 Control(SlopApi::ControlKind kind, std::string_view id, std::string_view label, std::string_view description, double min, double max, double step, std::uint32_t flags = SlopApi::kControlNone) {
    SlopApi::ControlDescriptorV1 control{}; control.kind = kind; control.flags = flags; Copy(control.controlId, id); Copy(control.label, label); Copy(control.description, description); control.minimumValue=min; control.maximumValue=max; control.stepValue=step; return control;
}
const SlopApi::ControlDescriptorV1 g_controls[] = {
    Control(SlopApi::ControlKind::Toggle, "enabled", "Enabled", "Enable mouse alignment assistance.", 0, 1, 1),
    Control(SlopApi::ControlKind::FloatSlider, "radius", "Alignment radius", "Maximum alignment radius in game units.", 1, 200, 1),
    Control(SlopApi::ControlKind::IntegerSlider, "idle-ms", "Idle delay", "Milliseconds to wait before alignment starts.", 10, 500, 10),
    Control(SlopApi::ControlKind::FloatSlider, "decay-rate", "Decay rate", "Rate at which alignment influence decays.", .5, 20, .5),
    Control(SlopApi::ControlKind::IntegerSlider, "poll-rate-hz", "Poll rate", "Input polling frequency in hertz.", 30, 500, 10),
    Control(SlopApi::ControlKind::IntegerSlider, "suppress-key", "Suppress key", "Virtual-key code that temporarily suppresses alignment.", 0, 255, 1, SlopApi::kControlAdvanced),
    Control(SlopApi::ControlKind::Toggle, "diagnostic-log", "Diagnostic logging", "Write diagnostic alignment events after restart.", 0, 1, 1, SlopApi::kControlRequiresRestart)
};
const SlopApi::ModuleDescriptorV1 g_module = [] {
    SlopApi::ModuleDescriptorV1 module{};
    Copy(module.moduleId, "AbsoluteZero");
    Copy(module.displayName, "AbsoluteZero");
    Copy(module.description, "Mouse alignment and centering options.");
    return module;
}();
const SlopApi::PageDescriptorV1 g_page = [] { SlopApi::PageDescriptorV1 page{}; Copy(page.moduleId, "AbsoluteZero"); Copy(page.pageId, "mouse-alignment"); Copy(page.displayName, "Mouse Alignment"); Copy(page.description, "Configure mouse alignment assistance behavior."); page.controlCount=static_cast<std::uint32_t>(std::size(g_controls)); page.controls=g_controls; page.readValue=Read; page.writeDraft=Write; page.apply=Apply; page.cancel=Cancel; return page; }();
} // namespace

namespace SlopSubscriber {
SlopApi::Result RegisterWith(QueryApiCallback query, const SlopApi::PageDescriptorV1& page) noexcept {
    if (!query) return SlopApi::Result::NotFound;
    const auto* api = query(SlopApi::kAbiVersion);
    if (!api) return SlopApi::Result::NotReady;
    if (!ValidApi(api)) return SlopApi::Result::Rejected;
    constexpr std::size_t registerModuleEnd = offsetof(SlopApi::ApiV1, registerModule) +
        sizeof(((SlopApi::ApiV1*)nullptr)->registerModule);
    if (api->structSize >= registerModuleEnd && api->registerModule) {
        const auto moduleResult = api->registerModule(&g_module);
        if (moduleResult != SlopApi::Result::Ok &&
            moduleResult != SlopApi::Result::Duplicate) {
            return moduleResult;
        }
    }
    return api->registerPage(&page);
}
void SetConfigurationAccess(ConfigurationAccess access) noexcept { g_access = access.get && access.apply && access.save ? access : ConfigurationAccess{ProductionGet, ProductionApply, ProductionSave}; }
void ResetForTests() noexcept { g_editing = false; g_rollback = {}; }
const SlopApi::PageDescriptorV1& Page() noexcept { return g_page; }
const SlopApi::ModuleDescriptorV1& Module() noexcept { return g_module; }
SlopApi::Result RegisterDiscoveredHost() noexcept {
    // The release host keeps the legacy v1 query export so research subscribers can
    // migrate without an ABI break. Prefer its current product DLL name.
    for (const wchar_t* name : {
             L"AbsoluteControlPanel.dll", L"AbsoluteControlPanelResearch.dll", L"SLOP.dll" }) {
        if (const auto module = GetModuleHandleW(name)) {
            if (const auto address = GetProcAddress(module, "SLOP_QueryApi")) return RegisterWith(reinterpret_cast<QueryApiCallback>(address), Page());
            return SlopApi::Result::NotFound;
        }
    }
    return SlopApi::Result::NotFound;
}
} // namespace SlopSubscriber
