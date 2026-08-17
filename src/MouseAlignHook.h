#pragma once

#include <cstdint>

namespace MouseAlignHook {
    using AccumulatorReader = bool(__cdecl*)(float*, float*) noexcept;
    using AccumulatorWriter = bool(__cdecl*)(float, float) noexcept;

    /// Install the trampoline hook on the rotational writer block.
    /// Returns true if the hook was successfully installed.
    bool Install();

    /// Use bounded accumulator operations from the module that owns the
    /// rotational writer capture. This prevents colliding trampolines and keeps
    /// the raw game pointer inside its owning DLL.
    void SetExternalAccumulatorProvider(AccumulatorReader reader,
                                        AccumulatorWriter writer);
    bool UsesExternalAccumulatorProvider();
    bool SupportsVirtualJoy();
    bool SourceAvailable();
    bool ReadAccumulator(float& yaw, float& pitch);
    bool WriteAccumulator(float yaw, float pitch);

    /// Returns the source object pointer (R13) captured by the hook.
    /// The mouse accumulator lives at GetSourcePtr()+0x4C (yaw) and +0x50 (pitch).
    uintptr_t GetSourcePtr();

    /// Returns the writer cluster base pointer (RAX = [R13+0x1E8]) captured by the hook.
    /// The signal hunter scans offsets from this base for the 0.0314 sentinel.
    uintptr_t GetWriterClusterPtr();

    /// Feed virtual accumulator values into the hook. If enabled=true, 
    /// the shellcode directly drives the ship steering offsets using these values.
    void SetVirtualJoy(bool enabled, float yaw, float pitch);

    float GetVirtualYaw();
    float GetVirtualPitch();
}
