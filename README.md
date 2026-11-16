# AbsoluteZero v1.5.0 — Mouse Steering for Starfield

AbsoluteZero is an SFSE plugin for mouse-flight alignment and optional locked-reticule steering.
It can gently return the ship's mouse-aim accumulator toward center after input stops, or keep the
reticule centered while translating mouse motion into direct ship steering.

Mechanically inspired by [Xilandro](https://www.nexusmods.com/starfield/users/4477498)'s
**Flight Controls Autoalign**. The implementation was developed independently.

## Features

- Smooth exponential idle centering with configurable radius, delay, and decay rate.
- **Locked Reticule mode**: keeps crosshair centered while translating mouse motion into direct ship steering, toggleable on the settings page and via remappable shortcut.
- **Built-in settings page**: one plain in-game page (default `F8`) with the tuning values and Apply / Save to INI / Reset to defaults.
- Shortcuts for Hold to suppress, Center on press, and Locked Reticule toggle are set in the INI. They accept keyboard combos and, with AbsoluteHOTAS installed, DirectInput joystick buttons / POV hats via AbsoluteInputBus.
- Hold-to-suppress binding for temporary unrestricted mouse aim in automatic mode.
- Optional one-shot **Center on press** activation mode alongside automatic idle centering.
- Standalone: no other mod is needed. The INI and live-tuning hotkeys work with the settings page turned off.
- Safe paired operation with AbsoluteHOTAS through shared writer seam and graceful backward-compatible fallback.
- Fail-closed runtime validation and optional diagnostics.

## Requirements

- Starfield `1.16.244`
- [SFSE](https://sfse.silverlock.org/) `0.2.21` or later
- Address Library is **not** required by AbsoluteZero.

## Installation

Install `AbsoluteZero-v1.5.0-Release.zip` with MO2 or Vortex, or place these files manually:

```text
Data\SFSE\Plugins\AbsoluteZero.dll
Data\SFSE\Plugins\AbsoluteZero.ini
```

Launch through SFSE and press `F8` in game to open the settings page. Use the mouse on it while the game is paused: in flight the game holds the cursor at the center of the screen. The arrow keys, Tab, Space and Enter work at any time.

## AbsoluteHOTAS compatibility and axis authority

AbsoluteZero coordinates with AbsoluteHOTAS safely without competing writer hooks:

- AbsoluteZero owns mouse pitch/yaw steering and idle centering.
- AbsoluteHOTAS yields its pitch/yaw stick bindings and embedded alignment path.
- AbsoluteHOTAS continues to own roll, strafe, throttle, ship buttons, profiles, and the single
  shared writer hook.
- AbsoluteZero uses bounded HOTAS accumulator operations and installs no competing trampoline.
- Seamlessly falls back to existing public AbsoluteHOTAS ApiV1 releases with zero conflicts while supporting synchronized ApiV2 execution.

Install both only when you want mouse steering to own pitch and yaw. A joystick cannot control
those two axes while AbsoluteZero is present. Native mouse alignment remains fully supported in paired mode.

## Configuration

Press the menu key (default `F8`) in game to open the settings page. It has checkboxes and sliders
for enable, Locked Reticule, activation mode, radius, idle delay, decay rate, polling rate and
logging, shows the current hotkeys read-only, and has four buttons: **Apply** (takes effect at
once), **Save to INI**, **Reset to defaults** and **Close**. While the page is open the game does
not receive mouse or keyboard input and the tuning hotkeys are ignored. Hotkeys themselves are
changed in the INI. The logging switch is read when the game starts.

The page draws with Dear ImGui through a D3D12 present hook. If it clashes with another overlay or
a frame-generation layer, set `bMenuEnabled = false`; no renderer hook is installed then.

Everything is also in `AbsoluteZero.ini`:

| Key | Default | Description |
|---|---:|---|
| `bEnabled` | `true` | Enable idle centering. |
| `fRadius` | `140.0` | Activation radius in the 0–200 accumulator range. |
| `iIdleMs` | `80` | Idle time before centering begins. |
| `fDecayRate` | `12.0` | Exponential centering speed. |
| `iPollRateHz` | `120` | Worker polling frequency. |
| `iSuppressKey` | `0x42` | Legacy hold-to-suppress virtual key (`0` disables). |
| `bSuppressKeyCtrl` / `bSuppressKeyAlt` / `bSuppressKeyShift` | `false` | Modifiers for legacy hold-to-suppress key. |
| `sSuppressBinding` | `keyboard:0x42;ctrl=0;alt=0;shift=0` | Full binding token (keyboard or DirectInput joystick button/hat). |
| `iActivationMode` | `0` | `0` = automatic after idle; `1` = center once when center key/button is pressed. |
| `iCenterKey` | `0x43` | Legacy one-shot center key used by mode `1`. |
| `bCenterKeyCtrl` / `bCenterKeyAlt` / `bCenterKeyShift` | `false` | Modifiers for legacy center key. |
| `sCenterBinding` | `keyboard:0x43;ctrl=0;alt=0;shift=0` | Full binding token for Center on press. |
| `bLogEnabled` | `false` | Write `AbsoluteZero.log`. |
| `bVirtualJoy` | `true` | Enable Locked Reticule mode. |
| `iVJoyToggleKey` | `0x60` | Legacy Locked Reticule toggle key (Numpad 0). |
| `bVJoyToggleCtrl` / `bVJoyToggleAlt` / `bVJoyToggleShift` | `false` / `true` / `false` | Modifiers for legacy toggle key. |
| `sVJoyToggleBinding` | `keyboard:0x60;ctrl=0;alt=1;shift=0` | Full binding token for Locked Reticule toggle. |

| `bMenuEnabled` | `true` | Built-in settings page. `false` installs no renderer hook (restart needed). |
| `iMenuKey` | `0x77` | Key that opens and closes the settings page (F8; `0` disables). |
| `bMenuKeyCtrl` / `bMenuKeyAlt` / `bMenuKeyShift` | `false` | Modifiers for the menu key. |

## Live tuning hotkeys

| Hotkey | Action |
|---|---|
| `Alt+Numpad+` / `Alt+Numpad-` | Radius ±2.0 |
| `Ctrl+Numpad+` / `Ctrl+Numpad-` | Idle delay ±10 ms |
| `Shift+Numpad+` / `Shift+Numpad-` | Decay rate ±0.5 |
| `Ctrl+Alt+0` | Toggle idle centering |
| `Alt+Numpad0` | Toggle Locked Reticule mode (default; remappable in the INI) |
| `Ctrl+Alt+S` | Save current settings |
| `F8` | Open or close the settings page (default; `iMenuKey`) |

Logging occurs only when `bLogEnabled=true`. Use the save command to preserve hotkey adjustments
across restarts.

## How it works

Standalone, AbsoluteZero captures Starfield's rotational-writer source and applies its mouse policy
through one validated hook. Beside a compatible AbsoluteHOTAS build, HOTAS retains that hook and
exposes bounded accumulator reads/writes; AbsoluteZero installs no second hook. Configuration is
copied under a mutex so UI edits and polling cannot race.

## Absolute suite

- [Absolute Control](https://github.com/SultanDesync/Absolute-Control) — shared native PauseMenu
  configuration host.
- [AbsoluteHOTAS](https://www.nexusmods.com/starfield/mods/16668) — HOTAS/HOSAS flight control.
- [Absolute Head Tracking](https://www.nexusmods.com/starfield/mods/17872) — OpenTrack-compatible
  rotational cockpit tracking.
- [Absolute Power](https://github.com/SultanDesync/Absolute-Power) — priority-based ship-power
  presets.
## Building

Requires CMake 3.30+, MSVC with C++23 support, and vcpkg:

```powershell
cmake --preset build-release
cmake --build build/release
```

Output is staged under `contrib/PluginRelease/Data/SFSE/Plugins`.

## License

GNU General Public License v3.0.

## Credits

- **Antigravity** — development
- **Xilandro** — mechanical inspiration
- **Starfield RE Team** — CommonLibSF
- **Ian Patterson / Stephen Abel** — SFSE
- **Omar Cornut** — Dear ImGui
- **Tsuda Kageyu** — MinHook
