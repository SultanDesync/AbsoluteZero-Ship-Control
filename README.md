# AbsoluteZero — Mouse Alignment Assist for Starfield

A lightweight SFSE plugin that gently centers your ship's mouse aim when idle during flight. No joystick input, no throttle injection, no binding wizard — just smooth exponential decay toward center.

Mechanically inspired by [Xilandro](https://www.nexusmods.com/starfield/users/4477498)'s **Flight Controls Autoalign** (out of support since 2024). Logic was independently developed.

> **Note**: This is a standalone sister project to [AbsoluteHOTAS](https://www.nexusmods.com/starfield/mods/210). If you already use AbsoluteHOTAS, you don't need this — enable `bAlignmentAssist = true` in AbsoluteHOTAS.ini instead.

---

The gameplay DLL remains independent. Absolute Control Panel and Absolute Workbench are optional
shared frontends for the same daughter-owned INI and live configuration.

## Absolute Control Panel integration (experimental)

The experimental subscriber branch registers a **Mouse Alignment** page containing the existing
enable switch, radius, idle delay, decay rate, poll rate, suppression key, and diagnostic logging
settings. The host renders those descriptors but AbsoluteZero continues to own validation, live
preview, persistence, and Cancel rollback.

The adapter dynamically discovers a loaded host and currently uses the host's preserved ABI-v1
research export. There is no import-library or load-time dependency. A missing or incompatible
Control Panel is a normal state: AbsoluteZero continues with Absolute Workbench, direct INI
editing, and its existing runtime hotkeys. This branch intentionally preserves those legacy paths
until the Control Panel SDK and product ABI are ready to freeze.

## Absolute suite

- [Absolute Workbench](https://github.com/SultanDesync/Absolute-Workbench) — optional
  shared configuration UI for this plugin and the other daughter mods.
- [AbsoluteHOTAS](https://github.com/SultanDesync/AbsoluteHOTAS) — complete HOTAS/HOSAS
  flight-control stack; it already contains alignment assistance, so do not load both.
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
AbsoluteHOTAS before touching the shared writer path. Its stoppable worker reads
mutex-protected configuration snapshots; UI and hotkey changes never race the
polling loop.

1. **Hook**: At startup, the plugin AOB-scans for Starfield's rotational writer block and installs a trampoline hook to capture the source object pointer.
2. **Alignment Assist**: A polling thread observes the captured mouse accumulator (`source+0x4C` yaw, `source+0x50` pitch). When the mouse is idle and within the configured radius, it applies exponential decay to gently center the aim.

## Compatibility

The configuration API remains queryable for status and editing even when the
AbsoluteHOTAS guard prevents gameplay hook installation.

- **Do NOT load alongside AbsoluteHOTAS** — both hook the same writer block. The plugin detects `AbsoluteHOTAS.dll` at load time and gracefully disables itself.
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
