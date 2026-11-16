#pragma once

// D3D12 ImGui overlay host, ported from AbsoluteHOTAS's retired workbench hook.
namespace UIHook {
    // Install the DXGI/D3D12 hooks. Call once from SFSEPluginLoad.
    bool Install();

    // Toggle the overlay. Safe to call from any thread.
    void ToggleUI();

    bool IsUIOpen();

    // Runs every frame on the render thread while the overlay is open.
    using DrawCallback = void(*)();
    void SetDrawCallback(DrawCallback cb);

    // Runs on every Present, open or not. Keep it cheap.
    using FrameCallback = void(*)();
    void SetFrameCallback(FrameCallback cb);
}
