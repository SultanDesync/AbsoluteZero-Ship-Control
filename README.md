# AbsoluteZero — Mouse Alignment Assist for Starfield

A lightweight SFSE plugin that gently centers your ship's mouse aim when idle during flight. No joystick input, no throttle injection, no binding wizard — just smooth exponential decay toward center.

Mechanically inspired by [Xilandro](https://www.nexusmods.com/starfield/users/4477498)'s **Flight Controls Autoalign** (out of support since 2024). Logic was independently developed.

> **Current compatibility**: This is the standalone owner of idle mouse centering for the modular
> Absolute suite. With the paired compatibility build of AbsoluteHOTAS installed, AbsoluteZero
> declares native mouse steering ownership: HOTAS releases pitch/yaw and its legacy alignment path,
> retains roll/strafe, and supplies bounded accumulator operations through one shared hook owner.

---

The gameplay DLL remains independent. Absolute Control Panel and Absolute Workbench are optional
shared frontends for the same daughter-owned INI and live configuration.

## Absolute Control Panel integration (experimental)

The current subscriber registers one **Mouse Alignment** page through the product ABI. It exposes
the existing enable switch, activation radius, idle delay, centering rate, suppression binding,
polling rate, and diagnostic logging settings. The suppression key uses the host's keyboard-capture
workflow instead of exposing a numeric virtual-key code. AbsoluteZero continues to own validation,
live preview, atomic INI persistence, and Cancel rollback.

The adapter discovers the canonical Control Panel, the current ResearchDev host, or the earlier
research artifact at SFSE data-load, with one bounded post-post-data-load retry for host readiness.
There is no import-library or load-time dependency. A missing or incompatible Control Panel is a
normal state: AbsoluteZero falls back to Absolute Workbench, direct INI editing, and its runtime
hotkeys. Control Panel open/capture state suppresses those local tuning hotkeys so two frontends
cannot edit concurrently.

## Absolute suite

- [Absolute Workbench](https://github.com/SultanDesync/Absolute-Workbench) — optional
  shared configuration UI for this plugin and the other daughter mods.
- [AbsoluteHOTAS](https://github.com/SultanDesync/AbsoluteHOTAS) — complete HOTAS/HOSAS
  flight-control stack. In paired compatibility mode, installing AbsoluteZero selects mouse
  pitch/yaw steering while HOTAS retains the remaining flight-control lanes.
- [Absolute Head Tracking](https://github.com/SultanDesync/Absolute-Head-Tracking) —
  standalone OpenTrack cockpit camera control compatible with AbsoluteZero.
- **Absolute Power** — priority-based ship-power presets and automation; its public
  repository is forthcoming.

## Requirements

- **Starfield** 1.16.242+
- **[SFSE](https://sfse.silverlock.org/)** 0.2.20+

## Installation

1. Extract `AbsoluteZero.dll` and `AbsoluteZero.ini` to `Data\SFSE\Plugins\`

## Configuration

Edit `Data\SFSE\Plugins\AbsoluteZero.ini`:

| Key | Default | Description |
|-----|---------|-------------|
| `bEnabled` | `true` | Enable/disable alignment assist |
| `fRadius` | `140.0` | Accumulator distance from center within which assist activates (0–200). Default is 70% of the circular range. |
| `iIdleMs` | `80` | Milliseconds the mouse must be idle before decay starts |
| `fDecayRate` | `12.0` | Exponential decay speed — higher values snap to center faster |
| `iPollRateHz` | `120` | Polling frequency in Hz |
| `iSuppressKey` | `0x42` (B) | Hold this key to suppress auto-centering. Set to `0` to disable. [Virtual key codes](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes) |
| `bLogEnabled` | `false` | Write debug info to `AbsoluteZero.log` |

## Configuration UI and live tuning

Install Absolute Control Panel to expose the experimental native Mouse Alignment page. Install
Absolute Workbench to retain its existing pages for Mouse Alignment,
Locked Reticule, and diagnostics. Workbench discovers
`AbsoluteZero.workbench.json`, queries the size-versioned
`AbsoluteZero_QueryApi(1)` POD interface, and asks this DLL to apply, save,
reload, and read back its own configuration. No C++ objects or gameplay ownership
cross the DLL boundary.

Without Workbench, the existing in-game hotkeys and direct INI editing remain
available:

Adjust settings on the fly without leaving the cockpit:

| Hotkey | Action | Step |
|--------|--------|------|
| `Alt + Numpad+` / `Numpad-` | Radius ↑/↓ | ±2.0 |
| `Ctrl + Numpad+` / `Numpad-` | Idle delay ↑/↓ | ±10 ms |
| `Shift + Numpad+` / `Numpad-` | Decay rate ↑/↓ | ±0.5 |
| `Ctrl + Alt + 0` | Toggle enable/disable | — |
| `Ctrl + Alt + S` | Save current settings to INI | — |

When `bLogEnabled=true`, changes are logged to `AbsoluteZero.log`. Use `Ctrl+Alt+S` to
persist your tuned values so they survive a restart.

While Workbench is open or capturing a key, AbsoluteZero updates hotkey edge
state without executing daughter actions. Closing the host therefore cannot
replay a chord that was pressed in the UI.

## How It Works

Runtime installation is deferred until SFSE reports that every plugin is loaded.
AbsoluteZero then checks both the installed file and loaded module for
AbsoluteHOTAS before touching the shared writer path. A compatible HOTAS exposes
bounded accumulator reads/writes and keeps ownership of the single machine-code
hook; no raw game pointer crosses the DLL boundary. Its stoppable worker reads
mutex-protected configuration snapshots; UI and hotkey changes never race the
polling loop.

1. **Capture**: Standalone, the plugin installs its own rotational-writer capture. Beside compatible AbsoluteHOTAS, it uses HOTAS's bounded accumulator seam and installs no competing trampoline.
2. **Alignment Assist**: A polling thread observes the captured mouse accumulator (`source+0x4C` yaw, `source+0x50` pitch). When the mouse is idle and within the configured radius, it applies exponential decay to gently center the aim.

## Compatibility

- **Paired AbsoluteHOTAS compatibility mode** — the presence of AbsoluteZero selects native mouse
  pitch/yaw steering. HOTAS ignores its pitch/yaw bindings, source-aim writer, and embedded alignment
  assist while retaining roll, strafe, throttle, buttons, and profiles. AbsoluteZero installs no
  second writer hook and owns idle centering. The legacy VirtualJoy mode is unavailable in this
  pairing; native mouse auto-centering remains supported.
- **Older or incompatible AbsoluteHOTAS builds** — AbsoluteZero remains queryable for configuration
  but fails closed instead of risking overlapping writer trampolines.
- Compatible with other SFSE plugins that don't modify the rotational writer block.

## Building

Requires CMake 3.30+, Ninja, MSVC (C++23), and vcpkg.

```bash
cmake --preset build-release
cmake --build build/release
```

Output: `contrib/PluginRelease/Data/SFSE/Plugins/AbsoluteZero.dll`

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.

## Credits

- **Antigravity** — Development
- **Xilandro** — Mechanical inspiration (Flight Controls Autoalign)
- **Starfield RE Team** — [CommonLibSF](https://github.com/Starfield-Reverse-Engineering/CommonLibSF)
- **Ian Patterson / Stephen Abel** — [SFSE](https://sfse.silverlock.org/)
