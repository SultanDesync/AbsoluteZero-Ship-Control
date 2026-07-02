#include "AlignmentAssist.h"
#include "MouseAlignHook.h"
#include "RuntimePaths.h"
#include <windows.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <format>
#include <string>

// ---- Static config ----
static AlignmentAssist::Config s_config;

// ---- SEH-safe memory access (must be standalone — not lambdas, not inlined) ----
// MSVC /EHsc forbids mixing SEH with C++ objects that have destructors.
#pragma optimize("", off)
static float SafeReadFloat(uintptr_t addr) {
    __try { return *(volatile float*)(addr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -999.0f; }
}

static void SafeWriteFloat(uintptr_t addr, float value) {
    __try { *(volatile float*)addr = value; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
#pragma optimize("", on)

// ---- Logging helper ----
static void AssistLog(const std::string& msg) {
    RuntimePaths::AppendLog("[AlignmentAssist]", msg);
}

// ---- Hotkey edge detection state ----
struct HotkeyState {
    bool prevAltPlus      = false;
    bool prevAltMinus     = false;
    bool prevCtrlPlus     = false;
    bool prevCtrlMinus    = false;
    bool prevShiftPlus    = false;
    bool prevShiftMinus   = false;
    bool prevToggle       = false;  // Ctrl+Alt+0
    bool prevVJoyToggle   = false;  // Alt+Numpad0
    bool prevSave         = false;  // Ctrl+Alt+S
};

static bool IsKeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static void LogConfigChange(const char* param, const std::string& value) {
    RuntimePaths::AppendLogAlways("[Tuning]",
        std::format("{} = {}  [radius={:.1f} idleMs={} decayRate={:.1f} enabled={}]",
            param, value,
            s_config.radius, s_config.idleMs, s_config.decayRate,
            s_config.enabled ? "true" : "false"));
}

static void ProcessHotkeys(HotkeyState& hk) {
    bool alt   = IsKeyDown(VK_MENU);
    bool ctrl  = IsKeyDown(VK_CONTROL);
    bool shift = IsKeyDown(VK_SHIFT);
    bool plus  = IsKeyDown(VK_ADD);
    bool minus = IsKeyDown(VK_SUBTRACT);
    bool sKey  = IsKeyDown('S');

    // Alt+Numpad+ / Alt+Numpad-  →  Radius ±2.0
    bool curAltPlus = alt && !ctrl && !shift && plus;
    if (curAltPlus && !hk.prevAltPlus) {
        s_config.radius = std::clamp(s_config.radius + 2.0f, 1.0f, 200.0f);
        LogConfigChange("fRadius", std::format("{:.1f}", s_config.radius));
    }
    hk.prevAltPlus = curAltPlus;

    bool curAltMinus = alt && !ctrl && !shift && minus;
    if (curAltMinus && !hk.prevAltMinus) {
        s_config.radius = std::clamp(s_config.radius - 2.0f, 1.0f, 200.0f);
        LogConfigChange("fRadius", std::format("{:.1f}", s_config.radius));
    }
    hk.prevAltMinus = curAltMinus;

    // Ctrl+Numpad+ / Ctrl+Numpad-  →  Idle delay ±10ms
    bool curCtrlPlus = ctrl && !alt && !shift && plus;
    if (curCtrlPlus && !hk.prevCtrlPlus) {
        s_config.idleMs = std::clamp(s_config.idleMs + 10, 10, 500);
        LogConfigChange("iIdleMs", std::to_string(s_config.idleMs));
    }
    hk.prevCtrlPlus = curCtrlPlus;

    bool curCtrlMinus = ctrl && !alt && !shift && minus;
    if (curCtrlMinus && !hk.prevCtrlMinus) {
        s_config.idleMs = std::clamp(s_config.idleMs - 10, 10, 500);
        LogConfigChange("iIdleMs", std::to_string(s_config.idleMs));
    }
    hk.prevCtrlMinus = curCtrlMinus;

    // Shift+Numpad+ / Shift+Numpad-  →  Decay rate ±0.5
    bool curShiftPlus = shift && !alt && !ctrl && plus;
    if (curShiftPlus && !hk.prevShiftPlus) {
        s_config.decayRate = std::clamp(s_config.decayRate + 0.5f, 0.5f, 20.0f);
        LogConfigChange("fDecayRate", std::format("{:.1f}", s_config.decayRate));
    }
    hk.prevShiftPlus = curShiftPlus;

    bool curShiftMinus = shift && !alt && !ctrl && minus;
    if (curShiftMinus && !hk.prevShiftMinus) {
        s_config.decayRate = std::clamp(s_config.decayRate - 0.5f, 0.5f, 20.0f);
        LogConfigChange("fDecayRate", std::format("{:.1f}", s_config.decayRate));
    }
    hk.prevShiftMinus = curShiftMinus;

    // Ctrl+Alt+0  →  Toggle enable/disable
    bool curToggle = ctrl && alt && !shift && IsKeyDown(0x30);
    if (curToggle && !hk.prevToggle) {
        s_config.enabled = !s_config.enabled;
        if (!s_config.enabled) MouseAlignHook::SetVirtualJoy(false, 0.0f, 0.0f);
        LogConfigChange("bEnabled", s_config.enabled ? "true" : "false");
    }
    hk.prevToggle = curToggle;

    // Toggle Virtual Joy mode
    bool toggleKey = IsKeyDown(s_config.vJoyToggleKey);
    bool curVJoyToggle = toggleKey && (!s_config.vJoyToggleAlt || alt);
    if (curVJoyToggle && !hk.prevVJoyToggle) {
        s_config.virtualJoy = !s_config.virtualJoy;
        if (!s_config.virtualJoy) MouseAlignHook::SetVirtualJoy(false, 0.0f, 0.0f);
        LogConfigChange("bVirtualJoy", s_config.virtualJoy ? "true" : "false");
    }
    hk.prevVJoyToggle = curVJoyToggle;

    // Ctrl+Alt+S  →  Save current config to INI
    bool curSave = ctrl && alt && !shift && sKey;
    if (curSave && !hk.prevSave) {
        AlignmentAssist::SaveConfig();
    }
    hk.prevSave = curSave;
}

// ---- Polling thread ----
static void PollThread() {
    float prevYaw   = 0.0f;
    float prevPitch = 0.0f;
    float vYaw      = 0.0f;
    float vPitch    = 0.0f;
    int   idleFrames = 0;
    auto  lastTime = std::chrono::steady_clock::now();
    HotkeyState hk{};

    AssistLog(std::format("Polling thread started @ {}Hz", s_config.pollRateHz));

    while (true) {
        std::this_thread::sleep_for(
            std::chrono::microseconds(1000000 / s_config.pollRateHz));

        // Process hotkeys every tick (edge-detected, very cheap)
        ProcessHotkeys(hk);

        if (!s_config.enabled) {
            MouseAlignHook::SetVirtualJoy(false, 0.0f, 0.0f);
            continue;
        }

        // Hold-to-suppress: skip decay while the suppress key is held
        if (s_config.suppressKey > 0 && (GetAsyncKeyState(s_config.suppressKey) & 0x8000)) {
            idleFrames = 0;
            continue;
        }

        uintptr_t src = MouseAlignHook::GetSourcePtr();
        if (!src) continue;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        dt = std::clamp(dt, 0.0001f, 0.1f);
        lastTime = now;

        if (s_config.virtualJoy) {
            // VJoy Mode: The crosshair is locked to center.
            // The vanilla accumulator is extracted synchronously by the hook,
            // which writes directly to g_vYaw and g_vPitch.

            // Read the currently accumulated values from the hook
            vYaw = MouseAlignHook::GetVirtualYaw();
            vPitch = MouseAlignHook::GetVirtualPitch();

            // Detect movement based on the delta
            constexpr float kEpsilon = 0.5f;
            bool moved = (std::abs(vYaw - prevYaw) > kEpsilon) || (std::abs(vPitch - prevPitch) > kEpsilon);

            prevYaw = vYaw;
            prevPitch = vPitch;

            if (moved) {
                idleFrames = 0;
            } else {
                idleFrames++;
            }

            // Apply Alignment Assist to the virtual accumulator
            int threshold = std::max(1, (s_config.pollRateHz * s_config.idleMs) / 1000);
            if (idleFrames >= threshold) {
                float dist = std::sqrt(vYaw * vYaw + vPitch * vPitch);
                if (dist > s_config.radius || dist <= 0.1f) {
                    // Outside assist radius
                } else {
                    float factor = std::exp(-s_config.decayRate * dt);
                    vYaw   *= factor;
                    vPitch *= factor;

                    if (std::abs(vYaw)   < 0.1f) vYaw   = 0.0f;
                    if (std::abs(vPitch) < 0.1f) vPitch = 0.0f;
                }
            }

            // Clamp virtual accumulator to the game's max UI radius (200.0f)
            // This allows the virtual accumulator to stay outside the auto-center radius and sustain turns.
            constexpr float kMaxVisualRadius = 200.0f;
            float currentDist = std::sqrt(vYaw * vYaw + vPitch * vPitch);
            if (currentDist > kMaxVisualRadius && currentDist > 0.001f) {
                float scale = kMaxVisualRadius / currentDist;
                vYaw *= scale;
                vPitch *= scale;
            }

            // Write the decayed value back to the global variables
            MouseAlignHook::SetVirtualJoy(true, vYaw, vPitch);

        } else {
            // Vanilla Mode
            MouseAlignHook::SetVirtualJoy(false, 0.0f, 0.0f);
            
            float curYaw   = SafeReadFloat(src + 0x4C);
            float curPitch = SafeReadFloat(src + 0x50);

            // Detect mouse movement
            constexpr float kEpsilon = 0.5f;
            bool moved = (std::abs(curYaw - prevYaw) > kEpsilon)
                      || (std::abs(curPitch - prevPitch) > kEpsilon);

            prevYaw   = curYaw;
            prevPitch = curPitch;

            if (moved) { idleFrames = 0; continue; }
            idleFrames++;

            // Wait for idle threshold
            int threshold = std::max(1, (s_config.pollRateHz * s_config.idleMs) / 1000);
            if (idleFrames < threshold) continue;

            // Check if within alignment radius
            float dist = std::sqrt(curYaw * curYaw + curPitch * curPitch);
            if (dist > s_config.radius || dist <= 0.1f) continue;

            // Exponential decay toward center
            float factor = std::exp(-s_config.decayRate * dt);
            float newYaw   = curYaw   * factor;
            float newPitch = curPitch * factor;

            // Snap to zero when close enough
            if (std::abs(newYaw)   < 0.1f) newYaw   = 0.0f;
            if (std::abs(newPitch) < 0.1f) newPitch = 0.0f;

            SafeWriteFloat(src + 0x4C, newYaw);
            SafeWriteFloat(src + 0x50, newPitch);

            prevYaw   = newYaw;
            prevPitch = newPitch;
        }
    }
}

// ---- Public API ----

void AlignmentAssist::Initialize(const Config& config) {
    s_config = config;
    AssistLog(std::format("Config: enabled={} vJoy={} radius={:.1f} idleMs={} decayRate={:.1f} pollHz={} suppressKey=0x{:02X}",
        config.enabled, config.virtualJoy, config.radius, config.idleMs, config.decayRate, config.pollRateHz, config.suppressKey));
}

void AlignmentAssist::Start() {
    std::thread(PollThread).detach();
    AssistLog("Polling thread spawned.");
}

void AlignmentAssist::SaveConfig() {
    auto ini = RuntimePaths::IniPath().string();
    const char* path = ini.c_str();

    auto writeStr = [&](const char* key, const char* val, const char* section = "MouseAlign") {
        WritePrivateProfileStringA(section, key, val, path);
    };
    auto writeFloat = [&](const char* key, float val, const char* section = "MouseAlign") {
        writeStr(key, std::to_string(val).c_str(), section);
    };
    auto writeInt = [&](const char* key, int val, const char* section = "MouseAlign") {
        writeStr(key, std::to_string(val).c_str(), section);
    };
    auto writeHex = [&](const char* key, int val, const char* section = "MouseAlign") {
        char buf[32]{};
        snprintf(buf, sizeof(buf), "0x%02X", val);
        writeStr(key, buf, section);
    };

    writeStr("bEnabled",    s_config.enabled ? "true" : "false");
    writeFloat("fRadius",   s_config.radius);
    writeInt("iIdleMs",     s_config.idleMs);
    writeFloat("fDecayRate",s_config.decayRate);
    writeInt("iPollRateHz", s_config.pollRateHz);
    writeHex("iSuppressKey",s_config.suppressKey);

    writeStr("bVirtualJoy", s_config.virtualJoy ? "true" : "false", "VirtualJoy");
    writeHex("iVJoyToggleKey", s_config.vJoyToggleKey, "VirtualJoy");
    writeStr("bVJoyToggleAlt", s_config.vJoyToggleAlt ? "true" : "false", "VirtualJoy");

    RuntimePaths::AppendLogAlways("[Tuning]",
        std::format("CONFIG SAVED to {} — radius={:.1f} idleMs={} decayRate={:.1f} enabled={} vJoy={}",
            ini, s_config.radius, s_config.idleMs, s_config.decayRate,
            s_config.enabled ? "true" : "false", s_config.virtualJoy ? "true" : "false"));
}
