#include "AlignmentAssist.h"

#include "Configuration.h"
#include "ControlPanelSubscriber.h"
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

    const bool virtualJoyToggle = KeyDown(config.vJoyToggleKey) &&
                                  (!config.vJoyToggleAlt || alt);
    if (!suppressed && virtualJoyToggle && !previous.virtualJoyToggle) {
        config.virtualJoy = !config.virtualJoy;
        if (!config.virtualJoy) {
            MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
        }
        changed = true;
        changedParameter = "bVirtualJoy";
        changedValue = config.virtualJoy ? "true" : "false";
    }
    previous.virtualJoyToggle = virtualJoyToggle;

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
            MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
            continue;
        }
        if (config.suppressKey > 0 && KeyDown(config.suppressKey)) {
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

        if (config.virtualJoy && MouseAlignHook::SupportsVirtualJoy()) {
            virtualYaw = MouseAlignHook::GetVirtualYaw();
            virtualPitch = MouseAlignHook::GetVirtualPitch();
            constexpr float epsilon = 0.5F;
            const bool moved = std::abs(virtualYaw - previousYaw) > epsilon ||
                               std::abs(virtualPitch - previousPitch) > epsilon;
            previousYaw = virtualYaw;
            previousPitch = virtualPitch;
            idleFrames = moved ? 0 : idleFrames + 1;
            const int threshold =
                std::max(1, (config.pollRateHz * config.idleMs) / 1000);
            if (idleFrames >= threshold) {
                const float distance = std::hypot(virtualYaw, virtualPitch);
                if (distance <= config.radius && distance > 0.1F) {
                    const float factor = std::exp(-config.decayRate * deltaTime);
                    virtualYaw *= factor;
                    virtualPitch *= factor;
                    if (std::abs(virtualYaw) < 0.1F) {
                        virtualYaw = 0.0F;
                    }
                    if (std::abs(virtualPitch) < 0.1F) {
                        virtualPitch = 0.0F;
                    }
                }
            }
            constexpr float maximumRadius = 200.0F;
            const float distance = std::hypot(virtualYaw, virtualPitch);
            if (distance > maximumRadius) {
                const float scale = maximumRadius / distance;
                virtualYaw *= scale;
                virtualPitch *= scale;
            }
            MouseAlignHook::SetVirtualJoy(true, virtualYaw, virtualPitch);
            continue;
        }

        MouseAlignHook::SetVirtualJoy(false, 0.0F, 0.0F);
        constexpr float epsilon = 0.5F;
        const bool moved = std::abs(currentYaw - previousYaw) > epsilon ||
                           std::abs(currentPitch - previousPitch) > epsilon;
        previousYaw = currentYaw;
        previousPitch = currentPitch;
        if (moved) {
            idleFrames = 0;
            continue;
        }
        ++idleFrames;
        const int threshold = std::max(1, (config.pollRateHz * config.idleMs) / 1000);
        if (idleFrames < threshold) {
            continue;
        }
        const float distance = std::hypot(currentYaw, currentPitch);
        if (distance > config.radius || distance <= 0.1F) {
            continue;
        }
        const float factor = std::exp(-config.decayRate * deltaTime);
        float nextYaw = currentYaw * factor;
        float nextPitch = currentPitch * factor;
        if (std::abs(nextYaw) < 0.1F) {
            nextYaw = 0.0F;
        }
        if (std::abs(nextPitch) < 0.1F) {
            nextPitch = 0.0F;
        }
        MouseAlignHook::WriteAccumulator(nextYaw, nextPitch);
        previousYaw = nextYaw;
        previousPitch = nextPitch;
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
