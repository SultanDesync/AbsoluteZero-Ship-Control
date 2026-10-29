# AbsoluteZero v1.4.0 — Mouse Steering for Starfield

AbsoluteZero is an SFSE plugin for mouse-flight alignment and optional locked-reticule steering.
It can gently return the ship's mouse-aim accumulator toward center after input stops, or keep the
reticule centered while translating mouse motion into direct ship steering.

Mechanically inspired by [Xilandro](https://www.nexusmods.com/starfield/users/4477498)'s
**Flight Controls Autoalign**. The implementation was developed independently.

## Features

- Smooth exponential idle centering with configurable radius, delay, and decay rate.
- **Locked Reticule mode**: keeps crosshair centered while translating mouse motion into direct ship steering, toggleable in-menu and via remappable shortcut.
- **Dedicated Bindings tab**: configure shortcuts for Hold to suppress, Center on press, and Locked Reticule toggle. Supports standard keyboard combos and physical DirectInput joystick buttons / POV hats via AbsoluteInputBus.
- **Interactive circular tuning lab**: responds continuously to mouse movement to preview configured activation radius, idle delay, and return speed in real time.
- **Multi-tab Absolute Control menu**: cleanly organized into Mouse Alignment (Tuning Lab + flight toggles) and Bindings tabs with unified Apply/Cancel draft transactions.
- Hold-to-suppress binding for temporary unrestricted mouse aim in automatic mode.
- Optional one-shot **Center on press** activation mode alongside automatic idle centering.
- Standalone runtime with direct INI and live-tuning hotkey operation when no shared frontend is installed.
- Safe paired operation with AbsoluteHOTAS through shared writer seam and graceful backward-compatible fallback.
- Fail-closed runtime validation and optional diagnostics.

## Requirements

- Starfield `1.16.242` or `1.16.244`
- [SFSE](https://sfse.silverlock.org/) `0.2.20` or later
- Absolute Control is recommended for the native in-game editor, but is not a gameplay dependency.
- Address Library is **not** required by AbsoluteZero.

## Installation

Install `AbsoluteZero-v1.4.0-Release.zip` with MO2 or Vortex, or place these files manually:

```text
Data\SFSE\Plugins\AbsoluteZero.dll
Data\SFSE\Plugins\AbsoluteZero.ini
Data\SFSE\Plugins\AbsoluteZero.workbench.json
```

With Absolute Control installed, launch through SFSE, open the Pause Menu, select **MOD OPTIONS**,
and choose **AbsoluteZero**.

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

The native Absolute Control page organizes settings into two tabs:
- **Tab 1 ("Mouse Alignment")**: Landing tab hosting the interactive Alignment Tuning Lab (2D circular flight envelope) alongside primary flight toggles (Enable auto-centering, Activation mode, Always lock reticle, Polling rate, Diagnostic logging) and centering sliders (Radius, Idle delay, Return rate).
- **Tab 2 ("Bindings")**: Dedicated input configuration for Hold to suppress, Center on press, and the Locked Reticule toggle. Supports keyboard shortcuts (with Ctrl, Alt, Shift modifiers) and physical DirectInput joystick buttons or POV hats.

AbsoluteZero owns live preview, validation, persistence, Apply, and Cancel across all tabs.

Direct configuration remains available in `AbsoluteZero.ini`:

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

The legacy Absolute Workbench pages remain available as an optional fallback. Direct INI editing and runtime hotkeys work without either host.

## Live tuning hotkeys

| Hotkey | Action |
|---|---|
| `Alt+Numpad+` / `Alt+Numpad-` | Radius ±2.0 |
| `Ctrl+Numpad+` / `Ctrl+Numpad-` | Idle delay ±10 ms |
| `Shift+Numpad+` / `Shift+Numpad-` | Decay rate ±0.5 |
| `Ctrl+Alt+0` | Toggle idle centering |
| `Alt+Numpad0` | Toggle Locked Reticule mode (default; remappable in MOD OPTIONS) |
| `Ctrl+Alt+S` | Save current settings |

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
