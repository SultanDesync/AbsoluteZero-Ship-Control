#pragma once

namespace SuiteState {
void SetInactiveByHOTAS(bool value) noexcept;
void SetHookInstalled(bool value) noexcept;
void SetHostActive(bool value) noexcept;
void SetWorkbenchSession(bool open, bool inputCaptureActive) noexcept;
[[nodiscard]] bool InactiveByHOTAS() noexcept;
[[nodiscard]] bool HookInstalled() noexcept;
[[nodiscard]] bool HostActive() noexcept;
[[nodiscard]] bool WorkbenchOpen() noexcept;
[[nodiscard]] bool InputCaptureActive() noexcept;
} // namespace SuiteState
