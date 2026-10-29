#include "InputBusClient.h"
#include "RuntimePaths.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <string>

namespace {

using QueryInputBus = const AbsoluteInputBusApi::ApiV1* (__cdecl*)(std::uint32_t) noexcept;

template <std::size_t N>
void CopyBuffer(char (&target)[N], std::string_view source) noexcept {
    const auto count = (std::min)(source.size(), N - 1);
    std::memcpy(target, source.data(), count);
    target[count] = '\0';
}

std::string ToLower(std::string_view value) {
    std::string result(value);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::optional<std::uint32_t> ParseNumber(std::string_view text) noexcept {
    if (text.empty()) return std::nullopt;
    std::uint32_t value{};
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec == std::errc{} && ptr == text.data() + text.size()) {
        return value;
    }
    return std::nullopt;
}

bool IsModifierKey(int vk) noexcept {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU;
}

bool IsMouseButton(int vk) noexcept {
    return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
           vk == VK_XBUTTON1 || vk == VK_XBUTTON2;
}

} // namespace

namespace JoystickBindingPolicy {

bool ValidToken(std::string_view token) noexcept {
    return Parse(token).has_value();
}

std::optional<JoystickBinding> Parse(std::string_view token) noexcept {
    if (token.empty() || token.size() > 256) return std::nullopt;
    const auto atPos = token.find('@');
    if (atPos == std::string_view::npos || atPos == 0 || atPos + 1 >= token.size()) {
        return std::nullopt;
    }

    const auto guidPart = token.substr(0, atPos);
    const auto controlPart = token.substr(atPos + 1);

    if (guidPart.size() < 4 || guidPart.contains(' ') || guidPart.contains('\r') ||
        guidPart.contains('\n')) {
        return std::nullopt;
    }

    const auto controlLower = ToLower(controlPart);
    std::uint32_t channelIndex = 0;

    if (controlLower.starts_with("button:")) {
        const auto numStr = controlPart.substr(std::string_view("button:").size());
        const auto num = ParseNumber(numStr);
        if (!num || *num < 1 || *num > 128) return std::nullopt;
        channelIndex = *num - 1; // 0..127
    } else if (controlLower.starts_with("pov:")) {
        const auto rest = controlPart.substr(std::string_view("pov:").size());
        const auto colonPos = rest.find(':');
        if (colonPos == std::string_view::npos) {
            const auto num = ParseNumber(rest);
            if (!num) return std::nullopt;
            if (*num >= 129 && *num <= 144) {
                channelIndex = *num - 1; // 128..143
            } else if (*num < 16) {
                channelIndex = 128 + *num;
            } else {
                return std::nullopt;
            }
        } else {
            const auto povIdxStr = rest.substr(0, colonPos);
            const auto dirStr = ToLower(rest.substr(colonPos + 1));
            const auto povIdx = ParseNumber(povIdxStr);
            if (!povIdx) return std::nullopt;

            std::uint32_t povNumber = *povIdx;
            if (povNumber >= 1 && povNumber <= 4) {
                povNumber -= 1;
            } else if (povNumber > 3) {
                return std::nullopt;
            }

            std::uint32_t dirOffset = 0;
            if (dirStr == "up" || dirStr == "north" || dirStr == "n" || dirStr == "0") dirOffset = 0;
            else if (dirStr == "right" || dirStr == "east" || dirStr == "e" || dirStr == "1") dirOffset = 1;
            else if (dirStr == "down" || dirStr == "south" || dirStr == "s" || dirStr == "2") dirOffset = 2;
            else if (dirStr == "left" || dirStr == "west" || dirStr == "w" || dirStr == "3") dirOffset = 3;
            else return std::nullopt;

            channelIndex = 128 + (povNumber * 4) + dirOffset;
        }
    } else {
        return std::nullopt;
    }

    return JoystickBinding{
        .persistentId = std::string(guidPart),
        .channelIndex = channelIndex,
        .originalToken = std::string(token),
    };
}

std::string FormatDisplay(std::string_view token,
                           std::string_view deviceProductName) noexcept {
    const auto parsed = Parse(token);
    if (!parsed) return std::string(token);

    std::string controlLabel;
    if (parsed->channelIndex < 128) {
        controlLabel = std::format("Button {}", parsed->channelIndex + 1);
    } else if (parsed->channelIndex < 144) {
        const auto povOffset = parsed->channelIndex - 128;
        const auto povIndex = povOffset / 4;
        const auto dirIndex = povOffset % 4;
        constexpr std::array<const char*, 4> dirs{"Up", "Right", "Down", "Left"};
        controlLabel = std::format("POV {} {}", povIndex + 1, dirs[dirIndex]);
    } else {
        controlLabel = std::format("Channel {}", parsed->channelIndex);
    }

    if (!deviceProductName.empty()) {
        return std::format("{} - {}", deviceProductName, controlLabel);
    }

    return std::format("Joy {}", controlLabel);
}

} // namespace JoystickBindingPolicy

InputBusClient& InputBusClient::Get() noexcept {
    static InputBusClient instance;
    return instance;
}

bool InputBusClient::Discover() noexcept {
    if (api_.load(std::memory_order_acquire) != nullptr) return true;
    const HMODULE hotas = GetModuleHandleW(L"AbsoluteHOTAS.dll");
    if (!hotas) return false;

    const FARPROC address = GetProcAddress(hotas, "AbsoluteHOTAS_QueryInputBusApi");
    if (!address) return false;

    const auto query = reinterpret_cast<QueryInputBus>(address);
    const auto* api = query(AbsoluteInputBusApi::kAbiVersion);
    if (!api) return false;

    constexpr std::size_t required =
        offsetof(AbsoluteInputBusApi::ApiV1, cancelCapture) +
        sizeof(api->cancelCapture);
    if (api->structSize < required ||
        api->abiVersion != AbsoluteInputBusApi::kAbiVersion ||
        !api->providerId ||
        std::string_view(api->providerId) != "absolute.hotas.input-bus" ||
        !api->getDeviceCount || !api->getDevice || !api->getSnapshot ||
        !api->beginCapture || !api->pollCapture || !api->cancelCapture) {
        return false;
    }

    api_.store(api, std::memory_order_release);
    RuntimePaths::AppendLogAlways("[InputBus]", "Connected to AbsoluteHOTAS Input Bus (ABI v1).");
    return true;
}

bool InputBusClient::IsAvailable() const noexcept {
    return api_.load(std::memory_order_acquire) != nullptr;
}

AbsoluteControlPanelApi::Result InputBusClient::BeginCapture(const char* controlId) noexcept {
    std::scoped_lock lock(captureMutex_);
    Discover();

    const auto* api = api_.load(std::memory_order_acquire);
    if (activeSessionId_ != 0 && api) {
        api->cancelCapture(activeSessionId_);
    }
    activeSessionId_ = 0;
    activeControlId_ = controlId ? controlId : "";

    // Baseline keyboard state so the triggering click or existing keys aren't captured
    for (int k = 1; k < 256; ++k) {
        keyBaseline_[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
    }
    captureBaselineTaken_ = true;

    if (api) {
        AbsoluteInputBusApi::CaptureRequestV1 request{};
        request.allowedControls = AbsoluteInputBusApi::kCaptureDigital;
        request.settleMilliseconds = 50;
        request.timeoutMilliseconds = 8000;
        CopyBuffer(request.consumerId, "absolute.zero");

        std::uint64_t session = 0;
        const auto res = api->beginCapture(&request, &session);
        if (res == AbsoluteInputBusApi::Result::Ok) {
            activeSessionId_ = session;
        }
    }

    return AbsoluteControlPanelApi::Result::Ok;
}

AbsoluteControlPanelApi::Result InputBusClient::PollCapture(
    const char* controlId,
    AbsoluteControlPanelApi::BindingCaptureV1* output) noexcept {
    if (!output || output->structSize < sizeof(AbsoluteControlPanelApi::BindingCaptureV1)) {
        return AbsoluteControlPanelApi::Result::InvalidArgument;
    }

    std::scoped_lock lock(captureMutex_);
    if (activeControlId_ != (controlId ? controlId : "")) {
        *output = {};
        output->state = AbsoluteControlPanelApi::BindingCaptureState::Idle;
        return AbsoluteControlPanelApi::Result::Ok;
    }

    // 1. Check keyboard keys first
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 && !keyBaseline_[VK_ESCAPE]) {
        if (activeSessionId_ != 0 && api_.load(std::memory_order_acquire)) {
            api_.load(std::memory_order_acquire)->cancelCapture(activeSessionId_);
        }
        activeSessionId_ = 0;
        activeControlId_.clear();
        *output = {};
        output->state = AbsoluteControlPanelApi::BindingCaptureState::Cancelled;
        CopyBuffer(output->detail, "Binding cancelled");
        return AbsoluteControlPanelApi::Result::Ok;
    }

    if (((GetAsyncKeyState(VK_BACK) & 0x8000) != 0 && !keyBaseline_[VK_BACK]) ||
        ((GetAsyncKeyState(VK_DELETE) & 0x8000) != 0 && !keyBaseline_[VK_DELETE])) {
        if (activeSessionId_ != 0 && api_.load(std::memory_order_acquire)) {
            api_.load(std::memory_order_acquire)->cancelCapture(activeSessionId_);
        }
        activeSessionId_ = 0;
        activeControlId_.clear();
        *output = {};
        output->state = AbsoluteControlPanelApi::BindingCaptureState::Captured;
        output->binding[0] = '\0';
        CopyBuffer(output->detail, "Binding cleared");
        return AbsoluteControlPanelApi::Result::Ok;
    }

    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    for (int k = 1; k < 256; ++k) {
        if (IsModifierKey(k) || IsMouseButton(k) || k == VK_ESCAPE || k == VK_BACK || k == VK_DELETE) {
            continue;
        }
        const bool isDown = (GetAsyncKeyState(k) & 0x8000) != 0;
        if (isDown && !keyBaseline_[k]) {
            // Valid keyboard key captured!
            if (activeSessionId_ != 0 && api_.load(std::memory_order_acquire)) {
                api_.load(std::memory_order_acquire)->cancelCapture(activeSessionId_);
            }
            activeSessionId_ = 0;
            activeControlId_.clear();

            std::string encoded = std::format("keyboard:0x{:02X};ctrl={};alt={};shift={}",
                k, ctrl ? 1 : 0, alt ? 1 : 0, shift ? 1 : 0);
            *output = {};
            output->state = AbsoluteControlPanelApi::BindingCaptureState::Captured;
            CopyBuffer(output->binding, encoded);
            CopyBuffer(output->detail, "Keyboard shortcut captured");
            return AbsoluteControlPanelApi::Result::Ok;
        }
        if (!isDown) {
            keyBaseline_[k] = false;
        }
    }

    // 2. Check AbsoluteHOTAS input bus if active
    const auto* api = api_.load(std::memory_order_acquire);
    if (api && activeSessionId_ != 0) {
        AbsoluteInputBusApi::CaptureResultV1 result{};
        const auto pollRes = api->pollCapture(activeSessionId_, &result);
        if (pollRes == AbsoluteInputBusApi::Result::Ok) {
            if (result.state == AbsoluteInputBusApi::CaptureState::Captured) {
                *output = {};
                output->state = AbsoluteControlPanelApi::BindingCaptureState::Captured;
                CopyBuffer(output->binding, result.binding.bindingText);
                CopyBuffer(output->detail, result.detail);
                activeSessionId_ = 0;
                activeControlId_.clear();
                return AbsoluteControlPanelApi::Result::Ok;
            }
            if (result.state == AbsoluteInputBusApi::CaptureState::Cancelled ||
                result.state == AbsoluteInputBusApi::CaptureState::TimedOut ||
                result.state == AbsoluteInputBusApi::CaptureState::Error) {
                *output = {};
                output->state = AbsoluteControlPanelApi::BindingCaptureState::Cancelled;
                CopyBuffer(output->detail, result.detail);
                activeSessionId_ = 0;
                activeControlId_.clear();
                return AbsoluteControlPanelApi::Result::Ok;
            }
        }
    }

    *output = {};
    output->state = AbsoluteControlPanelApi::BindingCaptureState::Capturing;
    CopyBuffer(output->detail, "Press any key or joystick button...");
    return AbsoluteControlPanelApi::Result::Ok;
}

AbsoluteControlPanelApi::Result InputBusClient::CancelCapture(const char*) noexcept {
    std::scoped_lock lock(captureMutex_);
    const auto* api = api_.load(std::memory_order_acquire);
    if (activeSessionId_ != 0 && api) {
        api->cancelCapture(activeSessionId_);
    }
    activeSessionId_ = 0;
    activeControlId_.clear();
    return AbsoluteControlPanelApi::Result::Ok;
}

bool InputBusClient::IsControlDown(std::string_view token) noexcept {
    const auto parsed = JoystickBindingPolicy::Parse(token);
    if (!parsed) return false;

    if (!Discover()) return false;
    const auto* api = api_.load(std::memory_order_acquire);
    if (!api) return false;

    const auto devCount = api->getDeviceCount();
    const auto targetId = ToLower(parsed->persistentId);

    for (std::uint32_t i = 0; i < devCount; ++i) {
        AbsoluteInputBusApi::DeviceInfoV1 info{};
        if (api->getDevice(i, &info) != AbsoluteInputBusApi::Result::Ok) continue;
        if (ToLower(info.persistentId) != targetId) continue;

        AbsoluteInputBusApi::DeviceSnapshotV1 snapshot{};
        if (api->getSnapshot(i, &snapshot) != AbsoluteInputBusApi::Result::Ok || !snapshot.connected) {
            return false;
        }

        const auto ch = parsed->channelIndex;
        if (ch >= AbsoluteInputBusApi::kDigitalControlCount) return false;
        const auto wordIdx = ch / 64;
        const auto bitIdx = ch % 64;
        return (snapshot.digitalDown[wordIdx] & (1ULL << bitIdx)) != 0;
    }
    return false;
}

bool InputBusClient::WasControlPressed(std::string_view token) noexcept {
    const auto parsed = JoystickBindingPolicy::Parse(token);
    if (!parsed) return false;

    if (!Discover()) return false;
    const auto* api = api_.load(std::memory_order_acquire);
    if (!api) return false;

    std::scoped_lock lock(trackingMutex_);
    const auto devCount = api->getDeviceCount();
    const auto targetId = ToLower(parsed->persistentId);

    for (std::uint32_t i = 0; i < devCount; ++i) {
        AbsoluteInputBusApi::DeviceInfoV1 info{};
        if (api->getDevice(i, &info) != AbsoluteInputBusApi::Result::Ok) continue;
        if (ToLower(info.persistentId) != targetId) continue;

        AbsoluteInputBusApi::DeviceSnapshotV1 snapshot{};
        if (api->getSnapshot(i, &snapshot) != AbsoluteInputBusApi::Result::Ok || !snapshot.connected) {
            return false;
        }

        const auto ch = parsed->channelIndex;
        if (ch >= AbsoluteInputBusApi::kDigitalControlCount) return false;

        auto& tracking = trackingByPersistentId_[targetId];
        if (!tracking.initialized || tracking.producerGeneration != snapshot.producerGeneration) {
            tracking.producerGeneration = snapshot.producerGeneration;
            std::ranges::copy(snapshot.pressCount, tracking.pressCountBaseline.begin());
            tracking.initialized = true;
            return false;
        }

        const auto currentCount = snapshot.pressCount[ch];
        const auto baselineCount = tracking.pressCountBaseline[ch];
        if (currentCount != baselineCount) {
            tracking.pressCountBaseline[ch] = currentCount;
            return true;
        }
    }
    return false;
}

std::string InputBusClient::GetDeviceProductName(std::string_view persistentId) noexcept {
    if (!Discover()) return {};
    const auto* api = api_.load(std::memory_order_acquire);
    if (!api) return {};

    const auto count = api->getDeviceCount();
    const auto target = ToLower(persistentId);
    for (std::uint32_t i = 0; i < count; ++i) {
        AbsoluteInputBusApi::DeviceInfoV1 info{};
        if (api->getDevice(i, &info) == AbsoluteInputBusApi::Result::Ok) {
            if (ToLower(info.persistentId) == target) {
                return std::string(info.productName);
            }
        }
    }
    return {};
}

std::string InputBusClient::FormatBinding(std::string_view token) noexcept {
    const auto parsed = JoystickBindingPolicy::Parse(token);
    if (!parsed) return std::string(token);
    const auto productName = GetDeviceProductName(parsed->persistentId);
    return JoystickBindingPolicy::FormatDisplay(token, productName);
}
