# AbsoluteZero v0.2.0 — Mouse Steering for Starfield

AbsoluteZero is an SFSE plugin for mouse-flight alignment and optional locked-reticule steering.
It can gently return the ship's mouse-aim accumulator toward center after input stops, or keep the
reticule centered while translating mouse motion into ship steering.

Mechanically inspired by [Xilandro](https://www.nexusmods.com/starfield/users/4477498)'s
**Flight Controls Autoalign**. The implementation was developed independently.

## Features

- Smooth exponential idle centering with configurable radius, delay, and decay rate.
- Hold-to-suppress binding for temporary unrestricted mouse aim.
- Optional Locked Reticule mode for centered-crosshair mouse steering.
- Live in-game tuning hotkeys and explicit save command.
- Native Absolute Control **Mouse Alignment** page with live settings and binding capture.
- Standalone runtime with direct INI and hotkey operation when no shared frontend is installed.
- Safe paired operation with current AbsoluteHOTAS builds through one shared writer owner.
- Fail-closed runtime validation and optional diagnostics.

## Requirements

- Starfield `1.16.242` or `1.16.244`
- [SFSE](https://sfse.silverlock.org/) `0.2.20` or later
- Absolute Control is recommended for the native in-game editor, but is not a gameplay dependency.
- Address Library is **not** required by AbsoluteZero.

## Installation

Install the archive with MO2 or Vortex, or place these files manually:

```text
Data\SFSE\Plugins\AbsoluteZero.dll
Data\SFSE\Plugins\AbsoluteZero.ini
```

With Absolute Control installed, launch through SFSE, open the Pause Menu, select **MOD OPTIONS**,
and choose **AbsoluteZero**.

## AbsoluteHOTAS compatibility and axis authority

AbsoluteZero and AbsoluteHOTAS cannot safely act as simultaneous pitch/yaw writers. Current paired
builds resolve that ownership explicitly:

- AbsoluteZero owns mouse pitch/yaw steering and idle centering.
- AbsoluteHOTAS yields its pitch/yaw stick bindings and embedded alignment path.
- AbsoluteHOTAS continues to own roll, strafe, throttle, ship buttons, profiles, and the single
  shared writer hook.
- AbsoluteZero uses bounded HOTAS accumulator operations and installs no competing trampoline.

Install both only when you want mouse steering to own pitch and yaw. A joystick cannot control
those two axes while AbsoluteZero is present. Locked Reticule/VirtualJoy mode is unavailable in the
paired HOTAS configuration; native mouse alignment remains supported.

Older or incompatible HOTAS builds cause AbsoluteZero to fail closed instead of risking overlapping
writer hooks.

## Configuration

The native Absolute Control page exposes alignment enablement, activation radius, idle delay,
centering rate, suppression binding, polling rate, and diagnostic logging. AbsoluteZero owns live
preview, validation, persistence, Apply, and Cancel.

Direct configuration remains available in `AbsoluteZero.ini`:

| Key | Default | Description |
|---|---:|---|
| `bEnabled` | `true` | Enable idle centering. |
| `fRadius` | `140.0` | Activation radius in the 0–200 accumulator range. |
| `iIdleMs` | `80` | Idle time before centering begins. |
| `fDecayRate` | `12.0` | Exponential centering speed. |
| `iPollRateHz` | `120` | Worker polling frequency. |
| `iSuppressKey` | `0x42` | Hold-to-suppress virtual key; `0` disables it. |
| `bLogEnabled` | `false` | Write `AbsoluteZero.log`. |
| `bVirtualJoy` | `false` | Enable standalone Locked Reticule mode. |
| `iVJoyToggleKey` | `0x60` | Locked Reticule toggle key. |
| `bVJoyToggleAlt` | `true` | Require Alt for that toggle. |

The legacy Absolute Workbench pages remain available as an optional fallback for Mouse Alignment,
Locked Reticule, and diagnostics. Direct INI editing and runtime hotkeys work without either host.

## Live tuning hotkeys

| Hotkey | Action |
|---|---|
| `Alt+Numpad+` / `Alt+Numpad-` | Radius ±2.0 |
| `Ctrl+Numpad+` / `Ctrl+Numpad-` | Idle delay ±10 ms |
| `Shift+Numpad+` / `Shift+Numpad-` | Decay rate ±0.5 |
| `Ctrl+Alt+0` | Toggle idle centering |
| `Alt+Numpad0` | Toggle standalone Locked Reticule mode |
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
