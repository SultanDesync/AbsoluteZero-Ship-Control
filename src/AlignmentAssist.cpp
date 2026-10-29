#include "AlignmentAssist.h"

#include "Configuration.h"
#include "ControlPanelSubscriber.h"
#include "InputBusClient.h"
#include "MouseAlignHook.h"
#include "RuntimePaths.h"
#include "SuiteState.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <string>
#include <thread>

namespace {
std::jthread g_pollThread;

struct HotkeyState {
    bool altPlus{};
    bool altMinus{};
    bool controlPlus{};
    bool controlMinus{};
    bool shiftPlus{};
    bool shiftMinus{};
    bool toggle{};
    bool virtualJoyToggle{};
    bool save{};
};

bool KeyDown(int virtualKey) {
    return virtualKey > 0 && virtualKey < 256 &&
           (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool ShortcutDown(int virtualKey, bool control, bool alt, bool shift) {
    return virtualKey > 0 && KeyDown(virtualKey) &&
           KeyDown(VK_CONTROL) == control && KeyDown(VK_MENU) == alt &&
           KeyDown(VK_SHIFT) == shift;
}

void LogChange(const char* parameter, const std::string& value,
               const Configuration::Config& config) {
    RuntimePaths::AppendLogAlways(
        "[Tuning]",
        std::format("{} = {} [radius={:.1f} idleMs={} decayRate={:.1f} enabled={}]", parameter,
                    value, config.radius, config.idleMs, config.decayRate,
                    config.enabled ? "true" : "false"));
}

void ProcessHotkeys(HotkeyState& previous, Configuration::Config& config) {
    const bool alt = KeyDown(VK_MENU);
    const bool control = KeyDown(VK_CONTROL);
    const bool shift = KeyDown(VK_SHIFT);
    const bool plus = KeyDown(VK_ADD);
    const bool minus = KeyDown(VK_SUBTRACT);
    const bool suppressed = ControlPanelSubscriber::IsMenuOpen() ||
                            ControlPanelSubscriber::IsInputCaptureActive() ||
                            SuiteState::WorkbenchOpen() ||
                            SuiteState::InputCaptureActive();
    bool changed{};
    const char* changedParameter{};
    std::string changedValue;

    const bool altPlus = alt && !control && !shift && plus;
    if (!suppressed && altPlus && !previous.altPlus) {
        config.radius = std::clamp(config.radius + 2.0F, 1.0F, 200.0F);
        changed = true;
        changedParameter = "fRadius";
        changedValue = std::format("{:.1f}", config.radius);
    }
    previous.altPlus = altPlus;

    const bool altMinus = alt && !control && !shift && minus;
    if (!suppressed && altMinus && !previous.altMinus) {
        config.radius = std::clamp(config.radius - 2.0F, 1.0F, 200.0F);
        changed = true;
        changedParameter = "fRadius";
        changedValue = std::format("{:.1f}", config.radius);
    }
    previous.altMinus = altMinus;

    const bool controlPlus = control && !alt && !shift && plus;
    if (!suppressed && controlPlus && !previous.controlPlus) {
        config.idleMs = std::clamp(config.idleMs + 10, 10, 500);
        changed = true;
        changedParameter = "iIdleMs";
        changedValue = std::to_string(config.idleMs);
    }
    previous.controlPlus = controlPlus;

    const bool controlMinus = control && !alt && !shift && minus;
    if (!suppressed && controlMinus && !previous.controlMinus) {
        config.idleMs = std::clamp(config.idleMs - 10, 10, 500);
        changed = true;
        changedParameter = "iIdleMs";
        changedValue = std::to_string(config.idleMs);
    }
    previous.controlMinus = controlMinus;

    const bool shiftPlus = shift && !alt && !control && plus;
    if (!suppressed && shiftPlus && !previous.shiftPlus) {
        config.decayRate = std::clamp(config.decayRate + 0.5F, 0.5F, 20.0F);
        changed = true;
        changedParameter = "fDecayRate";
        changedValue = std::format("{:.1f}", config.decayRate);
    }
    previous.shiftPlus = shiftPlus;

    const bool shiftMinus = shift && !alt && !control && minus;
    if (!suppressed && shiftMinus && !previous.shiftMinus) {
        config.decayRate = std::clamp(config.decayRate - 0.5F, 0.5F, 20.0F);
        changed = true;
        changedParameter = "fDecayRate";
        changedValue = std::format("{:.1f}", config.decayRate);
    }
    previous.shiftMinus = shiftMinus;


    const bool toggle = control && alt && !shift && KeyDown('0');
    if (!suppressed && toggle && !previous.toggle) {
        config.enabled = !config.enabled;
        if (!config.enabled) {
            MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
        }
        changed = true;
        changedParameter = "bEnabled";
        changedValue = config.enabled ? "true" : "false";
    }
    previous.toggle = toggle;

    const bool kbVJoyToggle = ShortcutDown(config.vJoyToggleKey,
        config.vJoyToggleControl, config.vJoyToggleAlt, config.vJoyToggleShift);
    const bool joyVJoyToggle = !config.vJoyToggleBinding.empty() &&
        InputBusClient::Get().WasControlPressed(config.vJoyToggleBinding);
    if (!suppressed && (joyVJoyToggle || (kbVJoyToggle && !previous.virtualJoyToggle))) {
        config.virtualJoy = !config.virtualJoy;
        if (!config.virtualJoy) {
            MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
        }
        changed = true;
        changedParameter = "bVirtualJoy";
        changedValue = config.virtualJoy ? "true" : "false";
    }
    previous.virtualJoyToggle = kbVJoyToggle;

    const bool save = control && alt && !shift && KeyDown('S');
    if (!suppressed && save && !previous.save) {
        const bool saved = Configuration::Save();
        RuntimePaths::AppendLogAlways("[Tuning]", saved ? "CONFIG SAVED to AbsoluteZero.ini"
                                                         : "CONFIG SAVE FAILED");
    }
    previous.save = save;

    if (changed && Configuration::Apply(config)) {
        LogChange(changedParameter, changedValue, config);
        config = Configuration::Get();
    }
}

void Poll(std::stop_token stopToken) {
    float previousYaw{};
    float previousPitch{};
    float virtualYaw{};
    float virtualPitch{};
    int idleFrames{};
    bool previousCenterDown{};
    bool centerRequested{};
    auto lastTime = std::chrono::steady_clock::now();
    HotkeyState hotkeys;
    RuntimePaths::AppendLog("[AlignmentAssist]", "Synchronized polling worker started.");

    while (!stopToken.stop_requested()) {
        auto config = Configuration::Get();
        std::this_thread::sleep_for(
            std::chrono::microseconds(1'000'000 / std::max(30, config.pollRateHz)));
        if (stopToken.stop_requested()) {
            break;
        }
        ProcessHotkeys(hotkeys, config);
        config = Configuration::Get();

        if (!config.enabled) {
            centerRequested = false;
            MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
            continue;
        }
        const bool centerInputSuppressed = ControlPanelSubscriber::IsMenuOpen() ||
            ControlPanelSubscriber::IsInputCaptureActive() ||
            SuiteState::WorkbenchOpen() || SuiteState::InputCaptureActive();
        const bool centerDown = ShortcutDown(config.centerKey, config.centerControl,
            config.centerAlt, config.centerShift);
        const bool joyCenterPressed = !config.centerBinding.empty() &&
            InputBusClient::Get().WasControlPressed(config.centerBinding);
        const bool centerPressed = !centerInputSuppressed &&
            config.centerOnPress && ((centerDown && !previousCenterDown) || joyCenterPressed);
        previousCenterDown = centerDown;
        if (centerPressed) centerRequested = true;
        if (centerInputSuppressed) centerRequested = false;
        if (!config.centerOnPress) centerRequested = false;

        const bool kbSuppress = ShortcutDown(config.suppressKey,
            config.suppressControl, config.suppressAlt, config.suppressShift);
        const bool joySuppress = !config.suppressBinding.empty() &&
            InputBusClient::Get().IsControlDown(config.suppressBinding);
        if (!config.centerOnPress && (kbSuppress || joySuppress)) {
            idleFrames = 0;
            continue;
        }

        float currentYaw{};
        float currentPitch{};
        if (!MouseAlignHook::ReadAccumulator(currentYaw, currentPitch)) {
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = std::clamp(
            std::chrono::duration<float>(now - lastTime).count(), 0.0001F, 0.1F);
        lastTime = now;

        const bool usingVirtualJoy =
            config.virtualJoy && MouseAlignHook::SupportsVirtualJoy();
        if (usingVirtualJoy) {
            virtualYaw = MouseAlignHook::GetVirtualYaw();
            virtualPitch = MouseAlignHook::GetVirtualPitch();
            constexpr float maximumRadius = 200.0F;
            const float virtualDistance = std::hypot(virtualYaw, virtualPitch);
            if (virtualDistance > maximumRadius) {
                const float scale = maximumRadius / virtualDistance;
                virtualYaw *= scale;
                virtualPitch *= scale;
            }
        }
        // Preserve the previous virtual-joystick ownership transition even on
        // frames that exit early for movement, idle delay, or one-shot state.
        MouseAlignHook::SetVirtualJoy(usingVirtualJoy, virtualYaw, virtualPitch);
        constexpr float epsilon = 0.5F;
        float observedYaw = usingVirtualJoy ? virtualYaw : currentYaw;
        float observedPitch = usingVirtualJoy ? virtualPitch : currentPitch;
        const bool moved = std::abs(observedYaw - previousYaw) > epsilon ||
                           std::abs(observedPitch - previousPitch) > epsilon;
        previousYaw = observedYaw;
        previousPitch = observedPitch;

        if (config.centerOnPress && !centerRequested) {
            idleFrames = 0;
            MouseAlignHook::SetVirtualJoy(usingVirtualJoy, virtualYaw, virtualPitch);
            continue;
        }
        if (config.centerOnPress && moved && !centerPressed) {
            // Fresh player input cancels an in-progress one-shot center. Writes
            // performed below update previousYaw/previousPitch, so the worker
            // does not mistake its own exponential decay for player movement.
            centerRequested = false;
            idleFrames = 0;
            MouseAlignHook::SetVirtualJoy(usingVirtualJoy, virtualYaw, virtualPitch);
            continue;
        }
        if (moved) {
            idleFrames = 0;
            continue;
        }
        if (!config.centerOnPress) {
            ++idleFrames;
            const int threshold =
                std::max(1, (config.pollRateHz * config.idleMs) / 1000);
            if (idleFrames < threshold) continue;
        }
        const float distance = std::hypot(observedYaw, observedPitch);
        if (distance > config.radius || distance <= 0.1F) {
            if (config.centerOnPress) centerRequested = false;
            MouseAlignHook::SetVirtualJoy(usingVirtualJoy, virtualYaw, virtualPitch);
            continue;
        }
        const float factor = std::exp(-config.decayRate * deltaTime);
        float nextYaw = observedYaw * factor;
        float nextPitch = observedPitch * factor;
        if (std::abs(nextYaw) < 0.1F) {
            nextYaw = 0.0F;
        }
        if (std::abs(nextPitch) < 0.1F) {
            nextPitch = 0.0F;
        }
        if (usingVirtualJoy) {
            virtualYaw = nextYaw;
            virtualPitch = nextPitch;
            MouseAlignHook::SetVirtualJoy(true, virtualYaw, virtualPitch);
        } else {
            MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
            MouseAlignHook::WriteAccumulator(nextYaw, nextPitch);
        }
        previousYaw = nextYaw;
        previousPitch = nextPitch;
        if (config.centerOnPress && nextYaw == 0.0F && nextPitch == 0.0F) {
            centerRequested = false;
        }
    }
    MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
}
} // namespace

namespace AlignmentAssist {
void Start() {
    if (!g_pollThread.joinable()) {
        g_pollThread = std::jthread(Poll);
    }
}

void Stop() {
    if (g_pollThread.joinable()) {
        g_pollThread.request_stop();
        g_pollThread.join();
    }
}
} // namespace AlignmentAssist
