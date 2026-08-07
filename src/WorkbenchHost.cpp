#include "WorkbenchHost.h"

#include "AbsoluteWorkbenchAPI.h"
#include "RuntimePaths.h"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <string_view>

namespace {
std::atomic<WorkbenchHost::Selection> g_selection{WorkbenchHost::Selection::Unavailable};

bool Valid(const AbsoluteWorkbenchApi::HostApiV1* api) {
    constexpr std::size_t minimum =
        offsetof(AbsoluteWorkbenchApi::HostApiV1, isInputCaptureActive) +
        sizeof(static_cast<AbsoluteWorkbenchApi::HostApiV1*>(nullptr)->isInputCaptureActive);
    return api && api->structSize >= minimum &&
           api->abiVersion == AbsoluteWorkbenchApi::kAbiVersion && api->moduleId &&
           std::string_view(api->moduleId) == AbsoluteWorkbenchApi::kModuleId && api->getHostMode &&
           api->requestOpen && api->isOpen && api->isInputCaptureActive;
}

WorkbenchHost::Selection Map(AbsoluteWorkbenchApi::HostMode mode) {
    switch (mode) {
    case AbsoluteWorkbenchApi::HostMode::Active:
        return WorkbenchHost::Selection::Active;
    case AbsoluteWorkbenchApi::HostMode::Suppressed:
        return WorkbenchHost::Selection::Suppressed;
    case AbsoluteWorkbenchApi::HostMode::Unavailable:
        return WorkbenchHost::Selection::Unavailable;
    case AbsoluteWorkbenchApi::HostMode::Incompatible:
        return WorkbenchHost::Selection::Incompatible;
    }
    return WorkbenchHost::Selection::Incompatible;
}
} // namespace

namespace WorkbenchHost {
Selection Select() noexcept {
    Selection selected = Selection::Unavailable;
    const HMODULE module = GetModuleHandleW(L"AbsoluteWorkbench.dll");
    if (module) {
        const FARPROC address = GetProcAddress(module, "AbsoluteWorkbench_QueryHostApi");
        if (!address) {
            selected = Selection::Incompatible;
        } else {
            const auto query = reinterpret_cast<AbsoluteWorkbenchApi::QueryHostApi>(address);
            const auto* api = query(AbsoluteWorkbenchApi::kAbiVersion);
            selected = Valid(api) ? Map(api->getHostMode()) : Selection::Incompatible;
        }
    }
    g_selection.store(selected);
    RuntimePaths::AppendLog(
        "[WorkbenchHost]",
        selected == Selection::Active
            ? "Absolute Workbench selected as the optional configuration host."
            : selected == Selection::Suppressed
                  ? "Absolute Workbench is present with graphics intentionally suppressed."
                  : selected == Selection::Unavailable
                        ? "Absolute Workbench is unavailable; hotkey and INI control remain active."
                        : "Absolute Workbench is incompatible; gameplay remains active without a shared frontend.");
    return selected;
}

Selection CurrentSelection() noexcept { return g_selection.load(); }
} // namespace WorkbenchHost
