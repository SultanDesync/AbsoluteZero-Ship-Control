#include "ControlPanelDiscovery.h"
#include "ControlPanelSubscriber.h"
#include "Configuration.h"
#include "SuppressBindingCodec.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

namespace {
using namespace AbsoluteControlPanelApi;
namespace Live = AbsoluteControlPanelExperimental;

ConfigPolicy::Config g_config = ConfigPolicy::Defaults();
int g_saves{};
bool g_applySucceeds{true};
ConfigPolicy::Config Get() { return g_config; }
bool Apply(const ConfigPolicy::Config& value) {
    if (!g_applySucceeds) return false;
    g_config = value;
    return true;
}
bool Save() { ++g_saves; return true; }

Result g_moduleResult = Result::Ok;
Result g_pageResult = Result::Ok;
std::vector<Result> g_pageResults;
std::vector<const PageDescriptorV1*> g_registeredPages;
const PageDescriptorV1* g_registeredPage{};
const ModuleDescriptorV1* g_registeredModule{};
Result g_unregisterResult = Result::Ok;
int g_moduleRegistrations{};
int g_unregisterCalls{};
bool g_open{};
bool g_capture{};
int g_resolveCalls{};
bool g_sawResearchDev{};

Result __cdecl RegisterPage(const PageDescriptorV1* page) noexcept {
    const auto registrationIndex = g_registeredPages.size();
    g_registeredPages.push_back(page);
    g_registeredPage = page;
    if (registrationIndex < g_pageResults.size()) {
        return g_pageResults[registrationIndex];
    }
    return g_pageResult;
}
Result __cdecl RegisterModule(const ModuleDescriptorV1* module) noexcept {
    ++g_moduleRegistrations;
    g_registeredModule = module;
    return g_moduleResult;
}
Result __cdecl Unregister(const char*) noexcept {
    ++g_unregisterCalls;
    return g_unregisterResult;
}
Result __cdecl Refresh(const char*, const char*) noexcept { return Result::Ok; }
std::uint8_t __cdecl IsOpen() noexcept { return g_open ? 1 : 0; }
std::uint8_t __cdecl IsCapture() noexcept { return g_capture ? 1 : 0; }

ApiV1 g_api{};
const Live::LiveChannelDescriptorV1* g_registeredLive{};
Live::Result g_liveResult{Live::Result::Ok};
int g_liveRegistrations{};

Live::Result __cdecl RegisterLive(
    const Live::LiveChannelDescriptorV1* descriptor) noexcept {
    g_registeredLive = descriptor;
    ++g_liveRegistrations;
    return g_liveResult;
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
    g_applySucceeds = true;
    g_liveResult = Live::Result::Ok;
    g_liveRegistrations = 0;
    g_moduleResult = Result::Ok;
    g_pageResult = Result::Ok;
    g_pageResults.clear();
    g_registeredPages.clear();
    g_registeredPage = nullptr;
    g_registeredModule = nullptr;
    g_unregisterResult = Result::Ok;
    g_moduleRegistrations = 0;
    g_unregisterCalls = 0;
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

    SuppressBindingCodec::KeyboardBinding parsedBinding;
    assert(SuppressBindingCodec::Parse(
        "keyboard:0x42;ctrl=1;alt=0;shift=1", parsedBinding) &&
        parsedBinding.virtualKey == 0x42 && parsedBinding.control && !parsedBinding.alt &&
        parsedBinding.shift);
    assert(!SuppressBindingCodec::Parse(
        "keyboard:0x42;ctrl=2;alt=0;shift=0", parsedBinding));
    assert(SuppressBindingCodec::Display(
        SuppressBindingCodec::KeyboardBinding{ 0x42, true, false, true }) == "Ctrl + Shift + B");
    assert(SuppressBindingCodec::Display(
        SuppressBindingCodec::KeyboardBinding{}) == "Unbound");
    assert(SuppressBindingCodec::DisplayToken({}) == "Unbound");

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
    assert(g_moduleRegistrations == 2);
    assert(g_registeredModule && g_registeredPages.size() == 2);
    assert(std::strcmp(g_registeredModule->moduleId, "absolute.zero") == 0);
    assert(std::strcmp(g_registeredPages[0]->pageId, "mouse-alignment") == 0);
    assert(std::strcmp(g_registeredPages[1]->pageId, "mouse-alignment-bindings") == 0);
    assert(ControlPanelSubscriber::IsHosted());
    g_open = true;
    g_capture = true;
    assert(ControlPanelSubscriber::IsMenuOpen());
    assert(ControlPanelSubscriber::IsInputCaptureActive());
    const auto registeredPageCount = g_registeredPages.size();
    const auto moduleRegistrationCount = g_moduleRegistrations;
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::Ok);
    assert(g_registeredPages.size() == registeredPageCount &&
        g_moduleRegistrations == moduleRegistrationCount);

    ResetHarness();
    g_moduleResult = Result::Duplicate;
    const auto moduleCollision =
        ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost);
    assert(moduleCollision == Result::Rejected);
    assert(DecideRegistration(moduleCollision, false) ==
        RegistrationDecision::UseFallback);
    assert(!ControlPanelSubscriber::IsHosted());
    assert(g_registeredPages.empty() && g_unregisterCalls == 0 &&
        g_moduleRegistrations == 1);
    g_moduleResult = Result::Ok;
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::Rejected);
    assert(g_registeredPages.empty() && g_unregisterCalls == 0 &&
        g_moduleRegistrations == 1);

    ResetHarness();
    g_pageResults = { Result::Ok, Result::Duplicate };
    const auto pageCollision =
        ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost);
    assert(pageCollision == Result::Rejected);
    assert(DecideRegistration(pageCollision, false) ==
        RegistrationDecision::UseFallback);
    assert(!ControlPanelSubscriber::IsHosted());
    assert(g_registeredPages.size() == 2 && g_unregisterCalls == 1);
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::Rejected);
    assert(g_registeredPages.size() == 2 && g_unregisterCalls == 1 &&
        g_moduleRegistrations == 1);

    ResetHarness();
    g_pageResult = Result::NotReady;
    g_unregisterResult = Result::Rejected;
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::Rejected);
    assert(!ControlPanelSubscriber::IsHosted());
    assert(g_registeredPages.size() == 1 && g_unregisterCalls == 1);
    g_pageResult = Result::Ok;
    g_unregisterResult = Result::Ok;
    assert(ControlPanelSubscriber::Testing::RegisterWithResolver(&ResolveCurrentHost) ==
        Result::Rejected);
    assert(g_registeredPages.size() == 1 && g_unregisterCalls == 1 &&
        g_moduleRegistrations == 1);

    Live::ExperimentalApiV1 liveApi;
    liveApi.registerLiveChannel = &RegisterLive;
    liveApi.unregisterModule = &UnregisterLive;
    liveApi.requestImmediateRefresh = &RefreshLive;
    g_liveResult = Live::Result::Duplicate;
    assert(ControlPanelSubscriber::Testing::RegisterLivePreview(&liveApi) ==
        Result::Rejected);
    g_liveResult = Live::Result::Ok;
    assert(ControlPanelSubscriber::Testing::RegisterLivePreview(&liveApi) ==
        Result::Ok);
    assert(ControlPanelSubscriber::Testing::RegisterLivePreview(&liveApi) ==
        Result::Ok && g_liveRegistrations == 2);
    assert(g_registeredLive &&
        g_registeredLive->kind == Live::ComponentKind::RadialResponse &&
        std::strcmp(g_registeredLive->channelId, "centering-lab") == 0 &&
        std::strcmp(g_registeredLive->radialResponse.radiusControlId,
            "radius") == 0);
    Live::LiveFrameV1 liveFrame;
    assert(g_registeredLive->readLiveFrame(nullptr, &liveFrame) ==
        Live::Result::Ok && liveFrame.radialResponse.available == 1 &&
        liveFrame.sequence > 0);

    const auto& landingPage = ControlPanelSubscriber::Testing::Page();
    assert(landingPage.controlCount == 8);
    const auto& bindingsPage = ControlPanelSubscriber::Testing::BindingsPage();
    assert(bindingsPage.controlCount == 3);

    const auto& binding = bindingsPage.controls[0];
    assert(binding.kind == ControlKind::InputBinding);
    assert((binding.flags & kBindingKeyboard) != 0);
    assert((binding.flags & kBindingClearable) != 0);
    assert((binding.flags & kBindingModifiers) != 0);

    assert(landingPage.controls[1].kind == ControlKind::Choice);
    ChoiceOptionV1 activationOptions[2]{};
    std::uint32_t activationOptionCount{};
    assert(landingPage.readChoiceOptions(nullptr, "activation-mode", activationOptions,
        2, &activationOptionCount) == Result::Ok);
    assert(activationOptionCount == 2 && activationOptions[0].value == 0 &&
        activationOptions[1].value == 1);

    ValueV1 value;
    assert(landingPage.readValue(nullptr, "enabled", &value) == Result::Ok &&
        value.kind == ValueKind::Boolean);
    assert(bindingsPage.readValue(nullptr, "suppress-binding", &value) == Result::Ok &&
        value.kind == ValueKind::String && std::strcmp(value.stringValue, "B") == 0);
    assert(bindingsPage.readValue(nullptr, "center-binding", &value) == Result::Ok &&
        value.kind == ValueKind::String && std::strcmp(value.stringValue, "C") == 0);
    assert(bindingsPage.readValue(nullptr, "locked-reticule-toggle-binding", &value) == Result::Ok &&
        value.kind == ValueKind::String &&
        std::strcmp(value.stringValue, "Alt + Numpad 0") == 0);
    assert(landingPage.readValue(nullptr, "missing", &value) == Result::NotFound);

    g_config.radius = 55.0F;
    auto radius = Float(60.0);
    assert(landingPage.writeDraft(nullptr, "radius", &radius) == Result::Ok &&
        g_config.radius == 60.0F);
    auto wrongRadius = Integer(70);
    assert(landingPage.writeDraft(nullptr, "radius", &wrongRadius) == Result::InvalidArgument);
    auto nan = Float(std::numeric_limits<double>::quiet_NaN());
    assert(landingPage.writeDraft(nullptr, "radius", &nan) == Result::InvalidArgument);
    auto encoded = String("keyboard:0x56;ctrl=0;alt=0;shift=0");
    assert(bindingsPage.writeDraft(nullptr, "suppress-binding", &encoded) == Result::Ok &&
        g_config.suppressKey == 0x56);
    auto modified = String("keyboard:0x47;ctrl=1;alt=0;shift=0");
    assert(bindingsPage.writeDraft(nullptr, "suppress-binding", &modified) ==
        Result::Ok && g_config.suppressKey == 0x47 && g_config.suppressControl);
    auto clear = String("");
    assert(bindingsPage.writeDraft(nullptr, "suppress-binding", &clear) == Result::Ok &&
        g_config.suppressKey == 0 && !g_config.suppressControl && !g_config.suppressAlt &&
        !g_config.suppressShift);
    auto pressMode = Integer(1);
    assert(landingPage.writeDraft(nullptr, "activation-mode", &pressMode) == Result::Ok &&
        g_config.centerOnPress);
    auto center = String("keyboard:0x4A;ctrl=0;alt=0;shift=0");
    assert(bindingsPage.writeDraft(nullptr, "center-binding", &center) == Result::Ok &&
        g_config.centerKey == 0x4A);
    auto reticule = String("keyboard:0x77;ctrl=0;alt=1;shift=1");
    assert(bindingsPage.writeDraft(nullptr, "locked-reticule-toggle-binding", &reticule) == Result::Ok &&
        g_config.vJoyToggleKey == 0x77 && !g_config.vJoyToggleControl &&
        g_config.vJoyToggleAlt && g_config.vJoyToggleShift);
    assert(std::strcmp(landingPage.controls[2].label, "Always lock reticle") == 0);
    assert(std::strcmp(bindingsPage.controls[2].label, "Locked reticle toggle") == 0);
    auto reticuleDisabled = Boolean(false);
    assert(landingPage.writeDraft(nullptr, "locked-reticule-enabled", &reticuleDisabled) == Result::Ok &&
        !g_config.virtualJoy);
    auto reticuleEnabled = Boolean(true);
    assert(landingPage.writeDraft(nullptr, "locked-reticule-enabled", &reticuleEnabled) == Result::Ok &&
        g_config.virtualJoy);
    auto enabled = Boolean(false);
    assert(landingPage.writeDraft(nullptr, "enabled", &enabled) == Result::Ok &&
        !g_config.enabled);
    g_applySucceeds = false;
    landingPage.cancel(nullptr);
    assert(g_config.radius == 60.0F && !g_config.enabled);
    g_applySucceeds = true;
    landingPage.cancel(nullptr);
    assert(g_config.radius == 55.0F && g_config.enabled &&
        !g_config.centerOnPress &&
        g_config.suppressKey == ConfigPolicy::Defaults().suppressKey &&
        g_config.centerKey == ConfigPolicy::Defaults().centerKey &&
        g_config.vJoyToggleKey == ConfigPolicy::Defaults().vJoyToggleKey &&
        g_config.vJoyToggleAlt == ConfigPolicy::Defaults().vJoyToggleAlt &&
        g_config.vJoyToggleShift == ConfigPolicy::Defaults().vJoyToggleShift &&
        g_config.virtualJoy == ConfigPolicy::Defaults().virtualJoy);

    assert(landingPage.writeDraft(nullptr, "radius", &radius) == Result::Ok);
    assert(landingPage.apply(nullptr) == Result::Ok && g_saves == 1);
    return 0;
}
