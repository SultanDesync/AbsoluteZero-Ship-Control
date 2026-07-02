#pragma once

namespace AlignmentAssist {
    struct Config {
        bool  enabled      = true;
        bool  virtualJoy   = false;   // Virtual Joystick mode (locks center, writes steering directly)
        int   vJoyToggleKey= 0x60;    // Virtual key code to toggle Virtual Joystick mode
        bool  vJoyToggleAlt= true;    // Require Alt for VJoy toggle
        float radius       = 130.0f;  // 65% of 200 — default covers most of the circular range
        int   idleMs       = 80;      // Mouse idle time before decay starts
        float decayRate    = 4.0f;    // Exponential decay speed (higher = faster snap)
        int   pollRateHz   = 120;     // Polling frequency
        int   suppressKey  = 0x42;    // Virtual key code for hold-to-suppress (default: B)
    };

    /// Initialize with the given config. Must be called before Start().
    void Initialize(const Config& config);

    /// Spawn the polling thread. Runs signal hunter until discovery, then alignment assist.
    void Start();

    /// Save current live-tuned config values back to the INI file.
    void SaveConfig();
}
