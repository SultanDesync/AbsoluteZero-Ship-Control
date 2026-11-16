#include "SettingsPage.h"

#include "Configuration.h"
#include "SuiteState.h"
#include "UIHook.h"

#include <imgui.h>

#include <windows.h>

#include <cstdint>
#include <format>
#include <string>

namespace {
// Everything here runs on the render thread.
Configuration::Config g_draft;
const char* g_status = "";
bool g_wasOpen{};
bool g_hotkeyWasDown{};

std::uint64_t g_hotkeyGeneration = ~0ULL;
int g_menuKey{};
bool g_menuControl{};
bool g_menuAlt{};
bool g_menuShift{};

bool KeyDown(int virtualKey) {
    return virtualKey > 0 && virtualKey < 256 && (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool GameFocused() {
    DWORD processId{};
    const HWND foreground = GetForegroundWindow();
    return foreground && GetWindowThreadProcessId(foreground, &processId) != 0 &&
           processId == GetCurrentProcessId();
}

std::string KeyText(int virtualKey, bool control, bool alt, bool shift,
                    const std::string& binding = {}) {
    if (virtualKey <= 0) {
        // Joystick bindings have no virtual key; show the stored token.
        return binding.empty() || binding.starts_with("keyboard:") ? "none" : binding;
    }
    std::string text;
    if (control) text += "Ctrl+";
    if (alt) text += "Alt+";
    if (shift) text += "Shift+";
    char name[64]{};
    const UINT scanCode = MapVirtualKeyA(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC);
    if (scanCode != 0 && GetKeyNameTextA(static_cast<LONG>(scanCode << 16), name, 64) > 0) {
        text += name;
    } else {
        text += std::format("0x{:02X}", virtualKey);
    }
    return text;
}

void ResetTunables(Configuration::Config& config) {
    const auto defaults = ConfigPolicy::Defaults();
    config.enabled = defaults.enabled;
    config.virtualJoy = defaults.virtualJoy;
    config.radius = defaults.radius;
    config.idleMs = defaults.idleMs;
    config.decayRate = defaults.decayRate;
    config.pollRateHz = defaults.pollRateHz;
    config.centerOnPress = defaults.centerOnPress;
    config.enableLog = defaults.enableLog;
}

// Every Present: watch the menu hotkey and publish the open state.
void OnFrame() {
    const auto generation = Configuration::Generation();
    if (generation != g_hotkeyGeneration) {
        const auto config = Configuration::Get();
        g_menuKey = config.menuKey;
        g_menuControl = config.menuControl;
        g_menuAlt = config.menuAlt;
        g_menuShift = config.menuShift;
        g_hotkeyGeneration = generation;
    }
    const bool down = KeyDown(g_menuKey) && KeyDown(VK_CONTROL) == g_menuControl &&
                      KeyDown(VK_MENU) == g_menuAlt && KeyDown(VK_SHIFT) == g_menuShift &&
                      GameFocused();
    if (down && !g_hotkeyWasDown) {
        UIHook::ToggleUI();
    }
    g_hotkeyWasDown = down;

    const bool open = UIHook::IsUIOpen();
    if (open && !g_wasOpen) {
        g_draft = Configuration::Get();
        g_status = "";
    }
    g_wasOpen = open;
    // Alignment hotkeys and press-to-centre stay quiet while the page is open.
    SuiteState::SetMenuOpen(open);
}

void Draw() {
    const auto applied = Configuration::Get();
    bool keepOpen = true;
    if (ImGui::Begin("AbsoluteZero 1.5.0", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("Mouse alignment enabled", &g_draft.enabled);
        ImGui::Checkbox("Locked Reticule (virtual joystick)", &g_draft.virtualJoy);
        ImGui::Checkbox("Centre only when the centre key is pressed", &g_draft.centerOnPress);
        // Ranges match ConfigPolicy::Sanitize.
        ImGui::SliderFloat("Radius", &g_draft.radius, 1.0F, 200.0F, "%.1f",
                           ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderInt("Idle delay (ms)", &g_draft.idleMs, 10, 500, "%d",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Decay rate", &g_draft.decayRate, 0.5F, 20.0F, "%.1f",
                           ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderInt("Poll rate (Hz)", &g_draft.pollRateHz, 30, 500, "%d",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Debug log (read at game start)", &g_draft.enableLog);

        ImGui::Separator();
        ImGui::TextUnformatted("Hotkeys (edit in AbsoluteZero.ini)");
        ImGui::Text("Suppress centring: %s",
                    KeyText(applied.suppressKey, applied.suppressControl, applied.suppressAlt,
                            applied.suppressShift, applied.suppressBinding).c_str());
        ImGui::Text("Centre: %s",
                    KeyText(applied.centerKey, applied.centerControl, applied.centerAlt,
                            applied.centerShift, applied.centerBinding).c_str());
        ImGui::Text("Locked Reticule toggle: %s",
                    KeyText(applied.vJoyToggleKey, applied.vJoyToggleControl,
                            applied.vJoyToggleAlt, applied.vJoyToggleShift,
                            applied.vJoyToggleBinding).c_str());
        ImGui::Text("This menu: %s",
                    KeyText(applied.menuKey, applied.menuControl, applied.menuAlt,
                            applied.menuShift).c_str());

        ImGui::Separator();
        if (ImGui::Button("Apply")) {
            g_status = Configuration::Apply(g_draft) ? "Applied." : "Apply failed.";
            g_draft = Configuration::Get();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to INI")) {
            // Apply first so the file matches what the page shows.
            const bool saved = Configuration::Apply(g_draft) && Configuration::Save();
            g_status = saved ? "Saved to AbsoluteZero.ini." : "Save failed.";
            g_draft = Configuration::Get();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to defaults")) {
            ResetTunables(g_draft);
            g_status = "Defaults loaded. Press Apply to use them.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            keepOpen = false;
        }

        if (g_draft != applied) {
            ImGui::TextUnformatted("Changes not applied yet.");
        } else if (Configuration::Dirty()) {
            ImGui::TextUnformatted("Applied settings are not saved to the INI yet.");
        }
        if (g_status[0] != '\0') {
            ImGui::TextUnformatted(g_status);
        }
    }
    ImGui::End();
    if (!keepOpen) {
        UIHook::ToggleUI();
    }
}
} // namespace

namespace SettingsPage {
bool Install() {
    UIHook::SetDrawCallback(&Draw);
    UIHook::SetFrameCallback(&OnFrame);
    return UIHook::Install();
}
} // namespace SettingsPage
