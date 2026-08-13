#include "SlopSubscriber.h"
#include "Configuration.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>

namespace {
ConfigPolicy::Config g_config = ConfigPolicy::Defaults();
int g_saves{};
ConfigPolicy::Config Get() noexcept { return g_config; }
bool Apply(const ConfigPolicy::Config& value) noexcept { g_config = value; return true; }
bool Save() noexcept { ++g_saves; return true; }

SlopApi::Result g_registerResult = SlopApi::Result::Ok;
const SlopApi::PageDescriptorV1* g_registered{};
const SlopApi::ModuleDescriptorV1* g_registeredModule{};
SlopApi::Result __cdecl Register(const SlopApi::PageDescriptorV1* page) noexcept { g_registered = page; return g_registerResult; }
SlopApi::Result __cdecl RegisterModule(const SlopApi::ModuleDescriptorV1* module) noexcept { g_registeredModule = module; return SlopApi::Result::Ok; }
SlopApi::Result __cdecl Unregister(const char*) noexcept { return SlopApi::Result::Ok; }
SlopApi::Result __cdecl Refresh(const char*, const char*) noexcept { return SlopApi::Result::Ok; }
SlopApi::ApiV1 g_api{};
const SlopApi::ApiV1* __cdecl GoodQuery(std::uint32_t version) noexcept { return version == SlopApi::kAbiVersion ? &g_api : nullptr; }
const SlopApi::ApiV1* __cdecl NullQuery(std::uint32_t) noexcept { return nullptr; }

SlopApi::ValueV1 Integer(int value) { SlopApi::ValueV1 v{}; v.kind=SlopApi::ValueKind::Integer; v.integerValue=value; return v; }
SlopApi::ValueV1 Float(double value) { SlopApi::ValueV1 v{}; v.kind=SlopApi::ValueKind::Float; v.floatValue=value; return v; }
SlopApi::ValueV1 Boolean(bool value) { SlopApi::ValueV1 v{}; v.kind=SlopApi::ValueKind::Boolean; v.booleanValue=value; return v; }
}
namespace Configuration {
Config Get() { return ::Get(); }
bool Apply(const Config& value) { return ::Apply(value); }
bool Save() { return ::Save(); }
}

int main() {
    static_assert(offsetof(SlopApi::ValueV1, structSize) == 0);
    static_assert(offsetof(SlopApi::ApiV1, registerPage) > offsetof(SlopApi::ApiV1, version));
    g_api = {}; g_api.moduleId = "SLOP"; g_api.registerPage = Register; g_api.unregisterModule = Unregister; g_api.requestRefresh = Refresh; g_api.registerModule = RegisterModule;
    SlopSubscriber::SetConfigurationAccess({Get, Apply, Save});
    const auto& page = SlopSubscriber::Page();
    const auto& module = SlopSubscriber::Module();
    assert(std::strcmp(module.displayName, "AbsoluteZero") == 0);
    assert(page.controlCount == 7 && std::strcmp(page.displayName, "Mouse Alignment") == 0);
    for (std::uint32_t i=0; i<page.controlCount; ++i) assert(page.controls[i].description[0] != '\0');
    assert(SlopSubscriber::RegisterWith(nullptr, page) == SlopApi::Result::NotFound);
    assert(SlopSubscriber::RegisterWith(NullQuery, page) == SlopApi::Result::NotReady);
    g_api.moduleId = "slop"; assert(SlopSubscriber::RegisterWith(GoodQuery, page) == SlopApi::Result::Rejected);
    g_api.moduleId = "SLOP"; g_api.registerPage = nullptr; assert(SlopSubscriber::RegisterWith(GoodQuery, page) == SlopApi::Result::Rejected);
    g_api.registerPage = Register; g_registerResult = SlopApi::Result::Duplicate;
    assert(SlopSubscriber::RegisterWith(GoodQuery, page) == SlopApi::Result::Duplicate && g_registered == &page && g_registeredModule == &module);
    g_registerResult = SlopApi::Result::Ok;

    SlopApi::ValueV1 value{};
    assert(page.readValue(nullptr, "enabled", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Boolean);
    assert(page.readValue(nullptr, "radius", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Float);
    assert(page.readValue(nullptr, "idle-ms", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Integer);
    assert(page.readValue(nullptr, "decay-rate", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Float);
    assert(page.readValue(nullptr, "poll-rate-hz", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Integer);
    assert(page.readValue(nullptr, "suppress-key", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Integer);
    assert(page.readValue(nullptr, "diagnostic-log", &value) == SlopApi::Result::Ok && value.kind == SlopApi::ValueKind::Boolean);
    assert(page.readValue(nullptr, "missing", &value) == SlopApi::Result::NotFound);
    g_config.radius = 55; // External change must become first-edit rollback state.
    auto radius = Float(60); assert(page.writeDraft(nullptr, "radius", &radius) == SlopApi::Result::Ok && g_config.radius == 60);
    auto wrong = Integer(70); assert(page.writeDraft(nullptr, "radius", &wrong) == SlopApi::Result::InvalidArgument && g_config.radius == 60);
    auto nan = Float(std::numeric_limits<double>::quiet_NaN()); assert(page.writeDraft(nullptr, "radius", &nan) == SlopApi::Result::InvalidArgument && g_config.radius == 60);
    auto out = Integer(510); assert(page.writeDraft(nullptr, "idle-ms", &out) == SlopApi::Result::InvalidArgument && g_config.radius == 60);
    assert(page.writeDraft(nullptr, "unknown", &radius) == SlopApi::Result::NotFound);
    auto enabled = Boolean(false); auto idle = Integer(100); auto decay = Float(10); auto poll = Integer(130); auto key = Integer(42); auto log = Boolean(true);
    assert(page.writeDraft(nullptr,"enabled",&enabled)==SlopApi::Result::Ok);
    assert(page.writeDraft(nullptr,"idle-ms",&idle)==SlopApi::Result::Ok);
    assert(page.writeDraft(nullptr,"decay-rate",&decay)==SlopApi::Result::Ok);
    assert(page.writeDraft(nullptr,"poll-rate-hz",&poll)==SlopApi::Result::Ok);
    assert(page.writeDraft(nullptr,"suppress-key",&key)==SlopApi::Result::Ok);
    assert(page.writeDraft(nullptr,"diagnostic-log",&log)==SlopApi::Result::Ok);
    page.cancel(nullptr); assert(g_config.radius == 55); // Entire external snapshot is restored.
    assert(page.writeDraft(nullptr, "radius", &radius) == SlopApi::Result::Ok);
    assert(page.apply(nullptr) == SlopApi::Result::Ok && g_saves == 1);
}
