#include "ControlPanelDiscovery.h"
#include "ControlPanelSubscriber.h"
#include "Configuration.h"
#include "SuppressBindingCodec.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <string_view>

namespace {
using namespace AbsoluteControlPanelApi;
namespace Live = AbsoluteControlPanelExperimental;

ConfigPolicy::Config g_config = ConfigPolicy::Defaults();
int g_saves{};
ConfigPolicy::Config Get() { return g_config; }
bool Apply(const ConfigPolicy::Config& value) { g_config = value; return true; }
bool Save() { ++g_saves; return true; }

Result g_moduleResult = Result::Ok;
Result g_pageResult = Result::Ok;
const PageDescriptorV1* g_registeredPage{};
const ModuleDescriptorV1* g_registeredModule{};
bool g_open{};
bool g_capture{};
int g_resolveCalls{};
bool g_sawResearchDev{};

Result __cdecl RegisterPage(const PageDescriptorV1* page) noexcept {
    g_registeredPage = page;
    return g_pageResult;
}
Result __cdecl RegisterModule(const ModuleDescriptorV1* module) noexcept {
    g_registeredModule = module;
    return g_moduleResult;
}
Result __cdecl Unregister(const char*) noexcept { return Result::Ok; }
Result __cdecl Refresh(const char*, const char*) noexcept { return Result::Ok; }
std::uint8_t __cdecl IsOpen() noexcept { return g_open ? 1 : 0; }
std::uint8_t __cdecl IsCapture() noexcept { return g_capture ? 1 : 0; }

ApiV1 g_api{};
const Live::LiveChannelDescriptorV1* g_registeredLive{};

Live::Result __cdecl RegisterLive(
    const Live::LiveChannelDescriptorV1* descriptor) noexcept {
    g_registeredLive = descriptor;
    return Live::Result::Ok;
}
Live::Result __cdecl UnregisterLive(const char*) noexcept {
    return Live::Result::Ok;
}
Live::Result __cdecl RefreshLive(
    const char*, const char*, const char*) noexcept {
    return Live::Result::Ok;
}

const ApiV1* __cdecl ResolveCurrentHost(const wchar_t* name) noexcept {
    ++g_resolveCalls;
    if (name && std::wstring_view(name) == L"AbsoluteControlPanelResearchDev.dll") {
        g_sawResearchDev = true;
        return &g_api;
    }
    return nullptr;
}

const ApiV1* __cdecl ResolveNoHost(const wchar_t*) noexcept { return nullptr; }

ValueV1 Boolean(bool value) {
    ValueV1 result;
    result.kind = ValueKind::Boolean;
    result.booleanValue = value ? 1U : 0U;
    return result;
}
ValueV1 Integer(int value) {
    ValueV1 result;
    result.kind = ValueKind::Integer;
    result.integerValue = value;
    return result;
}
ValueV1 Float(double value) {
    ValueV1 result;
    result.kind = ValueKind::Float;
    result.floatValue = value;
    return result;
}
ValueV1 String(std::string_view value) {
    ValueV1 result;
    result.kind = ValueKind::String;
    const auto count = (std::min)(value.size(), sizeof(result.stringValue) - 1);
    std::memcpy(result.stringValue, value.data(), count);
    return result;
}

void ResetHarness() {
    ControlPanelSubscriber::Testing::Reset();
    ControlPanelSubscriber::Testing::SetConfigurationAccess({ Get, Apply, Save });
    g_config = ConfigPolicy::Defaults();
    g_saves = 0;
    g_moduleResult = Result::Ok;
    g_pageResult = Result::Ok;
    g_registeredPage = nullptr;
    g_registeredModule = nullptr;
    g_open = false;
    g_capture = false;
    g_resolveCalls = 0;
    g_sawResearchDev = false;
    g_api = {};
    g_registeredLive = nullptr;
    g_api.moduleId = kModuleId.data();
    g_api.registerPage = &RegisterPage;
    g_api.unregisterModule = &Unregister;
    g_api.requestRefresh = &Refresh;
    g_api.registerModule = &RegisterModule;
    g_api.isOpen = &IsOpen;
    g_api.isInputCaptureActive = &IsCapture;
}
} // namespace

namespace Configuration {
Config Get() { return ::Get(); }
bool Apply(const Config& value) { return ::Apply(value); }
bool Save() { return ::Save(); }
}

int main() {
    using namespace AbsoluteControlPanelApi;
    using namespace ControlPanelSubscriber::Discovery;
    static_assert(sizeof(ControlDescriptorV1) == 392);
    static_assert(offsetof(PageDescriptorV1, readChoiceOptions) == 480);
    static_assert(kPageDescriptorV1BaseSize == 480);
    assert(DecideRegistration(Result::NotReady, false) ==
        RegistrationDecision::RetryAtPostPostDataLoad);
    assert(DecideRegistration(Result::NotReady, true) ==
        RegistrationDecision::UseFallback);

    int key{};
    assert(SuppressBindingCodec::Parse(
        "keyboard:0x42;ctrl=0;alt=0;shift=0", key) && key == 0x42);
    assert(!SuppressBindingCodec::Parse(
        "keyboard:0x42;ctrl=1;alt=0;shift=0", key));
    assert(SuppressBindingCodec::Display(0x42) == "B");
    assert(SuppressBindingCodec::Display(0) == "Unbound");

    ResetHarness();
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(nullptr) ==
        Result::InvalidArgument);
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveNoHost) ==
        Result::NotFound);

    ResetHarness();
    g_moduleResult = Result::NotReady;
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::NotReady);
    g_moduleResult = Result::Ok;
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::Ok);
    assert(g_sawResearchDev && g_resolveCalls >= 2);
    assert(g_registeredModule && g_registeredPage);
    assert(std::strcmp(g_registeredModule->moduleId, "absolute.zero") == 0);
    assert(std::strcmp(g_registeredPage->pageId, "mouse-alignment") == 0);
    assert(ControlPanelSubscriber::IsHosted());
    g_open = true;
    g_capture = true;
    assert(ControlPanelSubscriber::IsMenuOpen());
    assert(ControlPanelSubscriber::IsInputCaptureActive());

    Live::ExperimentalApiV1 liveApi;
    liveApi.registerLiveChannel = &RegisterLive;
    liveApi.unregisterModule = &UnregisterLive;
    liveApi.requestImmediateRefresh = &RefreshLive;
    assert(ControlPanelSubscriber::Testing::RegisterLivePreview(&liveApi) ==
        Result::Ok);
    assert(g_registeredLive &&
        g_registeredLive->kind == Live::ComponentKind::RadialResponse &&
        std::strcmp(g_registeredLive->channelId, "centering-lab") == 0 &&
        std::strcmp(g_registeredLive->radialResponse.radiusControlId,
            "radius") == 0);
    Live::LiveFrameV1 liveFrame;
    assert(g_registeredLive->readLiveFrame(nullptr, &liveFrame) ==
        Live::Result::Ok && liveFrame.radialResponse.available == 1 &&
        liveFrame.sequence > 0);

    const auto& page = ControlPanelSubscriber::Testing::Page();
    assert(page.controlCount == 9);
    const auto& binding = page.controls[5];
    assert(binding.kind == ControlKind::InputBinding);
    assert((binding.flags & kBindingKeyboard) != 0);
    assert((binding.flags & kBindingClearable) != 0);
    assert((binding.flags & kBindingModifiers) == 0);
    assert(page.controls[1].kind == ControlKind::Choice);
    ChoiceOptionV1 activationOptions[2]{};
    std::uint32_t activationOptionCount{};
    assert(page.readChoiceOptions(nullptr, "activation-mode", activationOptions,
        2, &activationOptionCount) == Result::Ok);
    assert(activationOptionCount == 2 && activationOptions[0].value == 0 &&
        activationOptions[1].value == 1);

    ValueV1 value;
    assert(page.readValue(nullptr, "enabled", &value) == Result::Ok &&
        value.kind == ValueKind::Boolean);
    assert(page.readValue(nullptr, "suppress-binding", &value) == Result::Ok &&
        value.kind == ValueKind::String && std::strcmp(value.stringValue, "B") == 0);
    assert(page.readValue(nullptr, "center-binding", &value) == Result::Ok &&
        value.kind == ValueKind::String && std::strcmp(value.stringValue, "C") == 0);
    assert(page.readValue(nullptr, "missing", &value) == Result::NotFound);

    g_config.radius = 55.0F;
    auto radius = Float(60.0);
    assert(page.writeDraft(nullptr, "radius", &radius) == Result::Ok &&
        g_config.radius == 60.0F);
    auto wrongRadius = Integer(70);
    assert(page.writeDraft(nullptr, "radius", &wrongRadius) == Result::InvalidArgument);
    auto nan = Float(std::numeric_limits<double>::quiet_NaN());
    assert(page.writeDraft(nullptr, "radius", &nan) == Result::InvalidArgument);
    auto encoded = String("keyboard:0x56;ctrl=0;alt=0;shift=0");
    assert(page.writeDraft(nullptr, "suppress-binding", &encoded) == Result::Ok &&
        g_config.suppressKey == 0x56);
    auto modified = String("keyboard:0x47;ctrl=1;alt=0;shift=0");
    assert(page.writeDraft(nullptr, "suppress-binding", &modified) ==
        Result::InvalidArgument);
    auto clear = String("");
    assert(page.writeDraft(nullptr, "suppress-binding", &clear) == Result::Ok &&
        g_config.suppressKey == 0);
    auto pressMode = Integer(1);
    assert(page.writeDraft(nullptr, "activation-mode", &pressMode) == Result::Ok &&
        g_config.centerOnPress);
    auto center = String("keyboard:0x4A;ctrl=0;alt=0;shift=0");
    assert(page.writeDraft(nullptr, "center-binding", &center) == Result::Ok &&
        g_config.centerKey == 0x4A);
    auto enabled = Boolean(false);
    assert(page.writeDraft(nullptr, "enabled", &enabled) == Result::Ok &&
        !g_config.enabled);
    page.cancel(nullptr);
    assert(g_config.radius == 55.0F && g_config.enabled &&
        !g_config.centerOnPress &&
        g_config.suppressKey == ConfigPolicy::Defaults().suppressKey &&
        g_config.centerKey == ConfigPolicy::Defaults().centerKey);

    assert(page.writeDraft(nullptr, "radius", &radius) == Result::Ok);
    assert(page.apply(nullptr) == Result::Ok && g_saves == 1);
    return 0;
}
