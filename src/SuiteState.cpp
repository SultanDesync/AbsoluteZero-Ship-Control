#include "SuiteState.h"

#include <atomic>

namespace {
std::atomic<bool> g_inactiveByHOTAS{};
std::atomic<bool> g_hookInstalled{};
std::atomic<bool> g_hostActive{};
std::atomic<bool> g_workbenchOpen{};
std::atomic<bool> g_inputCaptureActive{};
} // namespace

namespace SuiteState {
void SetInactiveByHOTAS(bool value) noexcept { g_inactiveByHOTAS.store(value); }
void SetHookInstalled(bool value) noexcept { g_hookInstalled.store(value); }
void SetHostActive(bool value) noexcept { g_hostActive.store(value); }
void SetWorkbenchSession(bool open, bool inputCaptureActive) noexcept {
    g_workbenchOpen.store(open);
    g_inputCaptureActive.store(inputCaptureActive);
}
bool InactiveByHOTAS() noexcept { return g_inactiveByHOTAS.load(); }
bool HookInstalled() noexcept { return g_hookInstalled.load(); }
bool HostActive() noexcept { return g_hostActive.load(); }
bool WorkbenchOpen() noexcept { return g_workbenchOpen.load(); }
bool InputCaptureActive() noexcept { return g_inputCaptureActive.load(); }
} // namespace SuiteState
