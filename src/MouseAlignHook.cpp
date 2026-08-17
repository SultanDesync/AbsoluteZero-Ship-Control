#include "MouseAlignHook.h"
#include "RuntimePaths.h"
#include <windows.h>
#include <cstring>
#include <cmath>
#include <string>
#include <format>

// ---- Global pointer storage (accessed by shellcode via absolute address) ----
static volatile uintptr_t g_sourcePtr = 0;          // R13 — source object containing mouse accumulator
static volatile uintptr_t g_writerClusterPtr = 0;   // RAX — writer cluster base ([R13+0x1E8])
static MouseAlignHook::AccumulatorReader g_externalAccumulatorReader{};
static MouseAlignHook::AccumulatorWriter g_externalAccumulatorWriter{};

// ---- Virtual Joy State ----
volatile uint8_t g_vJoyEnabled = 0;
volatile float   g_vYaw = 0.0f;
volatile float   g_vPitch = 0.0f;
volatile float   g_vYawNorm = 0.0f;
volatile float   g_vPitchNorm = 0.0f;
volatile float   g_vRadius = 200.0f; // Max vanilla crosshair circle is 200.0f


// ---- Logging helper ----
static void HookLog(const std::string& msg) {
    RuntimePaths::AppendLog("[MouseAlignHook]", msg);
}

extern "C" void VirtualJoySync() {
    if (!g_vJoyEnabled) return;

    uintptr_t src = MouseAlignHook::GetSourcePtr();
    if (!src) return;

    float* pYaw = (float*)(src + 0x4C);
    float* pPitch = (float*)(src + 0x50);

    // Synchronously extract the delta that accumulated this frame
    g_vYaw += *pYaw;
    g_vPitch += *pPitch;

    // Zero it out to stop the UI crosshair from moving
    *pYaw = 0.0f;
    *pPitch = 0.0f;

    if (g_vRadius > 0.001f) {
        g_vYawNorm = g_vYaw / g_vRadius;
        g_vPitchNorm = g_vPitch / g_vRadius;
    }
}

// ---- Find .text section ----
static bool GetTextSection(uintptr_t& textStart, size_t& textSize) {
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) return false;

    uintptr_t              moduleBase = (uintptr_t)hModule;
    IMAGE_DOS_HEADER*      dosHeader  = (IMAGE_DOS_HEADER*)moduleBase;
    IMAGE_NT_HEADERS*      ntHeaders  = (IMAGE_NT_HEADERS*)(moduleBase + dosHeader->e_lfanew);
    IMAGE_SECTION_HEADER*  section    = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        std::string name((char*)section[i].Name, 8);
        if (name.find(".text") != std::string::npos) {
            textStart = moduleBase + section[i].VirtualAddress;
            textSize  = section[i].Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

// ---- Allocate executable memory within ±2GB of target for short JMP ----
static uint8_t* AllocateNear(uintptr_t targetAddr) {
    uintptr_t allocBase = targetAddr > 0x70000000 ? targetAddr - 0x70000000 : 0x10000;
    for (uintptr_t addr = allocBase; addr < targetAddr + 0x70000000; addr += 0x10000) {
        uint8_t* mem = (uint8_t*)VirtualAlloc((void*)addr, 4096,
            MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (mem) return mem;
    }
    return nullptr;
}

// ---- Install the trampoline hook ----
// Hooks at the rotational writer block: lea rdx,[rax+58] (offset +7 within the 30-byte block).
// At this site: RAX = [R13+0x1E8] = writer cluster base, R13 = source object.
// The trampoline:
//   1. Replays original lea rdx,[rax+58]
//   2. Captures R13 → g_sourcePtr (source object with mouse accumulator)
//   3. Captures RAX → g_writerClusterPtr (writer cluster base, for signal hunter)
//   4. Replays clobbered vmovss [rdx],xmm0
//   5. JMPs back
static bool InstallTrampoline(uintptr_t hookAddr) {
    uint8_t* tramp = AllocateNear(hookAddr);
    if (!tramp) {
        HookLog("CRITICAL: Could not allocate trampoline memory.");
        return false;
    }

    int idx = 0;

    // 1. Replay original: lea rdx,[rax+58]  (4 bytes: 48 8D 50 58)
    tramp[idx++] = 0x48; tramp[idx++] = 0x8D;
    tramp[idx++] = 0x50; tramp[idx++] = 0x58;

    // 2. Save RCX + flags
    tramp[idx++] = 0x51;  // push rcx
    tramp[idx++] = 0x9C;  // pushfq

    // 3. Capture R13 → g_sourcePtr
    //    mov rcx, imm64 (&g_sourcePtr)
    tramp[idx++] = 0x48; tramp[idx++] = 0xB9;
    uintptr_t srcAddr = (uintptr_t)&g_sourcePtr;
    memcpy(&tramp[idx], &srcAddr, 8); idx += 8;
    //    mov [rcx], r13
    tramp[idx++] = 0x4C; tramp[idx++] = 0x89;
    tramp[idx++] = 0x29;

    // 4. Capture RAX → g_writerClusterPtr
    //    mov rcx, imm64 (&g_writerClusterPtr)
    tramp[idx++] = 0x48; tramp[idx++] = 0xB9;
    uintptr_t clusterAddr = (uintptr_t)&g_writerClusterPtr;
    memcpy(&tramp[idx], &clusterAddr, 8); idx += 8;
    //    mov [rcx], rax
    tramp[idx++] = 0x48; tramp[idx++] = 0x89;
    tramp[idx++] = 0x01;

    // 5. Restore flags + RCX
    tramp[idx++] = 0x9D;  // popfq
    tramp[idx++] = 0x59;  // pop rcx

    // 5.5. Synchronous extraction call
    // Save volatile integer registers: RAX, RCX, RDX, R8, R9, R10, R11
    tramp[idx++] = 0x50; // push rax
    tramp[idx++] = 0x51; // push rcx
    tramp[idx++] = 0x52; // push rdx
    tramp[idx++] = 0x41; tramp[idx++] = 0x50; // push r8
    tramp[idx++] = 0x41; tramp[idx++] = 0x51; // push r9
    tramp[idx++] = 0x41; tramp[idx++] = 0x52; // push r10
    tramp[idx++] = 0x41; tramp[idx++] = 0x53; // push r11

    // Save XMM0-XMM5 (6 registers * 16 bytes = 96 bytes = 0x60)
    tramp[idx++] = 0x48; tramp[idx++] = 0x83; tramp[idx++] = 0xEC; tramp[idx++] = 0x60; // sub rsp, 0x60

    tramp[idx++] = 0x0F; tramp[idx++] = 0x11; tramp[idx++] = 0x04; tramp[idx++] = 0x24; // movups [rsp], xmm0
    tramp[idx++] = 0x0F; tramp[idx++] = 0x11; tramp[idx++] = 0x4C; tramp[idx++] = 0x24; tramp[idx++] = 0x10; // movups [rsp+0x10], xmm1
    tramp[idx++] = 0x0F; tramp[idx++] = 0x11; tramp[idx++] = 0x54; tramp[idx++] = 0x24; tramp[idx++] = 0x20; // movups [rsp+0x20], xmm2
    tramp[idx++] = 0x0F; tramp[idx++] = 0x11; tramp[idx++] = 0x5C; tramp[idx++] = 0x24; tramp[idx++] = 0x30; // movups [rsp+0x30], xmm3
    tramp[idx++] = 0x0F; tramp[idx++] = 0x11; tramp[idx++] = 0x64; tramp[idx++] = 0x24; tramp[idx++] = 0x40; // movups [rsp+0x40], xmm4
    tramp[idx++] = 0x0F; tramp[idx++] = 0x11; tramp[idx++] = 0x6C; tramp[idx++] = 0x24; tramp[idx++] = 0x50; // movups [rsp+0x50], xmm5

    // Call VirtualJoySync
    tramp[idx++] = 0x48; tramp[idx++] = 0x83; tramp[idx++] = 0xEC; tramp[idx++] = 0x28; // sub rsp, 0x28 (shadow space)

    tramp[idx++] = 0x48; tramp[idx++] = 0xB8; // mov rax, imm64
    uintptr_t syncAddr = (uintptr_t)&VirtualJoySync;
    memcpy(&tramp[idx], &syncAddr, 8); idx += 8;

    tramp[idx++] = 0xFF; tramp[idx++] = 0xD0; // call rax

    tramp[idx++] = 0x48; tramp[idx++] = 0x83; tramp[idx++] = 0xC4; tramp[idx++] = 0x28; // add rsp, 0x28 (shadow space)

    // Restore XMM0-XMM5
    tramp[idx++] = 0x0F; tramp[idx++] = 0x10; tramp[idx++] = 0x04; tramp[idx++] = 0x24; // movups xmm0, [rsp]
    tramp[idx++] = 0x0F; tramp[idx++] = 0x10; tramp[idx++] = 0x4C; tramp[idx++] = 0x24; tramp[idx++] = 0x10; // movups xmm1, [rsp+0x10]
    tramp[idx++] = 0x0F; tramp[idx++] = 0x10; tramp[idx++] = 0x54; tramp[idx++] = 0x24; tramp[idx++] = 0x20; // movups xmm2, [rsp+0x20]
    tramp[idx++] = 0x0F; tramp[idx++] = 0x10; tramp[idx++] = 0x5C; tramp[idx++] = 0x24; tramp[idx++] = 0x30; // movups xmm3, [rsp+0x30]
    tramp[idx++] = 0x0F; tramp[idx++] = 0x10; tramp[idx++] = 0x64; tramp[idx++] = 0x24; tramp[idx++] = 0x40; // movups xmm4, [rsp+0x40]
    tramp[idx++] = 0x0F; tramp[idx++] = 0x10; tramp[idx++] = 0x6C; tramp[idx++] = 0x24; tramp[idx++] = 0x50; // movups xmm5, [rsp+0x50]

    tramp[idx++] = 0x48; tramp[idx++] = 0x83; tramp[idx++] = 0xC4; tramp[idx++] = 0x60; // add rsp, 0x60

    // Restore volatile integer registers
    tramp[idx++] = 0x41; tramp[idx++] = 0x5B; // pop r11
    tramp[idx++] = 0x41; tramp[idx++] = 0x5A; // pop r10
    tramp[idx++] = 0x41; tramp[idx++] = 0x59; // pop r9
    tramp[idx++] = 0x41; tramp[idx++] = 0x58; // pop r8
    tramp[idx++] = 0x5A; // pop rdx
    tramp[idx++] = 0x59; // pop rcx
    tramp[idx++] = 0x58; // pop rax

    // 6. Custom writer block replacement
    // push r8
    tramp[idx++] = 0x41; tramp[idx++] = 0x50;
    // push r9
    tramp[idx++] = 0x41; tramp[idx++] = 0x51;

    // Replay roll: vmovss [rdx], xmm0
    tramp[idx++] = 0xC5; tramp[idx++] = 0xFA;
    tramp[idx++] = 0x11; tramp[idx++] = 0x02;

    // Replay vert strafe: vmovss [rdx+4], xmm2
    tramp[idx++] = 0xC5; tramp[idx++] = 0xFA;
    tramp[idx++] = 0x11; tramp[idx++] = 0x52; tramp[idx++] = 0x04;

    // mov r8, imm64 (&g_vJoyEnabled)
    tramp[idx++] = 0x49; tramp[idx++] = 0xB8;
    uintptr_t vjoyAddr = (uintptr_t)&g_vJoyEnabled;
    memcpy(&tramp[idx], &vjoyAddr, 8); idx += 8;

    // cmp byte ptr [r8], 1
    tramp[idx++] = 0x41; tramp[idx++] = 0x80; tramp[idx++] = 0x38; tramp[idx++] = 0x01;

    // je vjoy_active
    tramp[idx++] = 0x74;
    int jeOffsetIdx = idx;
    tramp[idx++] = 0x00;

    // --- Vanilla path ---
    // vmovss [rax+0x60], xmm3
    tramp[idx++] = 0xC5; tramp[idx++] = 0xFA; tramp[idx++] = 0x11; tramp[idx++] = 0x58; tramp[idx++] = 0x60;
    // vmovss [rax+0x64], xmm1
    tramp[idx++] = 0xC5; tramp[idx++] = 0xFA; tramp[idx++] = 0x11; tramp[idx++] = 0x48; tramp[idx++] = 0x64;

    // jmp done
    tramp[idx++] = 0xEB;
    int jmpDoneOffsetIdx = idx;
    tramp[idx++] = 0x00;

    // --- VJoy path ---
    tramp[jeOffsetIdx] = (uint8_t)(idx - jeOffsetIdx - 1);

    // mov r8, imm64 (&g_vYawNorm)
    tramp[idx++] = 0x49; tramp[idx++] = 0xB8;
    uintptr_t vyawAddr = (uintptr_t)&g_vYawNorm;
    memcpy(&tramp[idx], &vyawAddr, 8); idx += 8;

    // mov r9d, dword ptr [r8]
    tramp[idx++] = 0x45; tramp[idx++] = 0x8B; tramp[idx++] = 0x08;

    // mov dword ptr [rax+0x60], r9d
    tramp[idx++] = 0x44; tramp[idx++] = 0x89; tramp[idx++] = 0x48; tramp[idx++] = 0x60;

    // mov r8, imm64 (&g_vPitchNorm)
    tramp[idx++] = 0x49; tramp[idx++] = 0xB8;
    uintptr_t vpitchAddr = (uintptr_t)&g_vPitchNorm;
    memcpy(&tramp[idx], &vpitchAddr, 8); idx += 8;

    // mov r9d, dword ptr [r8]
    tramp[idx++] = 0x45; tramp[idx++] = 0x8B; tramp[idx++] = 0x08;

    // mov dword ptr [rax+0x64], r9d
    tramp[idx++] = 0x44; tramp[idx++] = 0x89; tramp[idx++] = 0x48; tramp[idx++] = 0x64;

    // --- Done ---
    tramp[jmpDoneOffsetIdx] = (uint8_t)(idx - jmpDoneOffsetIdx - 1);

    // pop r9
    tramp[idx++] = 0x41; tramp[idx++] = 0x59;
    // pop r8
    tramp[idx++] = 0x41; tramp[idx++] = 0x58;

    // 7. JMP back to hookAddr + 23
    tramp[idx++] = 0xE9;
    int32_t jmpBack = (int32_t)((hookAddr + 23) - ((uintptr_t)&tramp[idx] + 4));
    memcpy(&tramp[idx], &jmpBack, 4); idx += 4;

    // Patch original site with JMP to trampoline (5 bytes: E9 xx xx xx xx)
    // We clobber 23 bytes total (lea=4 + 4 vmovss=19 = 23 bytes), but we only need a 5-byte JMP.
    // The remaining 18 bytes are NOPed for safety.
    DWORD oldProt;
    VirtualProtect((void*)hookAddr, 23, PAGE_EXECUTE_READWRITE, &oldProt);
    uint8_t* site = (uint8_t*)hookAddr;
    site[0] = 0xE9;
    int32_t jmpToTramp = (int32_t)((uintptr_t)tramp - (hookAddr + 5));
    memcpy(&site[1], &jmpToTramp, 4);
    // NOP the remaining 18 bytes
    for (int i = 5; i < 23; i++) {
        site[i] = 0x90;
    }
    VirtualProtect((void*)hookAddr, 23, oldProt, &oldProt);

    HookLog(std::format("Trampoline installed at 0x{:X}, shellcode at 0x{:X} ({} bytes)",
        hookAddr, (uintptr_t)tramp, idx));
    return true;
}

// ---- Public API ----

bool MouseAlignHook::Install() {
    HookLog("=== AbsoluteZero hook installation ===");

    uintptr_t moduleBase = (uintptr_t)GetModuleHandle(NULL);

    uintptr_t textStart = 0;
    size_t    textSize  = 0;
    if (!GetTextSection(textStart, textSize)) {
        HookLog("CRITICAL: Could not find .text section!");
        RuntimePaths::AppendLogAlways("[MouseAlignHook]", "CRITICAL: Could not find .text section!");
        return false;
    }

    HookLog(std::format(".text section: base=0x{:X} size=0x{:X}", textStart, textSize));

    // AOB scan for the 30-byte rotational writer block
    const uint8_t writerBlock[] = {
        0x49, 0x8B, 0x85, 0xE8, 0x01, 0x00, 0x00,   // mov rax,[r13+1E8]
        0x48, 0x8D, 0x50, 0x58,                       // lea rdx,[rax+58]     ← HOOK SITE
        0xC5, 0xFA, 0x11, 0x02,                       // vmovss [rdx],xmm0
        0xC5, 0xFA, 0x11, 0x52, 0x04,                 // vmovss [rdx+4],xmm2
        0xC5, 0xFA, 0x11, 0x58, 0x60,                 // vmovss [rdx+60],xmm3
        0xC5, 0xFA, 0x11, 0x48, 0x64                  // vmovss [rdx+64],xmm1
    };

    uintptr_t blockAddr = 0;
    for (size_t i = 0; i + sizeof(writerBlock) < textSize; i++) {
        if (memcmp((void*)(textStart + i), writerBlock, sizeof(writerBlock)) == 0) {
            blockAddr = textStart + i;
            break;
        }
    }

    if (!blockAddr) {
        HookLog("CRITICAL: Rotational writer block not found. The game may have updated.");
        RuntimePaths::AppendLogAlways("[MouseAlignHook]",
            "CRITICAL: Rotational writer block AOB not found. Hook installation failed.");
        return false;
    }

    HookLog(std::format("Writer block found at Starfield.exe+0x{:X}",
        (unsigned int)(blockAddr - moduleBase)));

    // Hook at lea rdx,[rax+58] (offset +7 within the block)
    uintptr_t hookAddr = blockAddr + 7;
    if (!InstallTrampoline(hookAddr)) {
        return false;
    }

    HookLog("=== Hook installation complete ===");
    RuntimePaths::AppendLogAlways("[MouseAlignHook]",
        std::format("Hook installed at Starfield.exe+0x{:X}", (unsigned int)(hookAddr - moduleBase)));
    return true;
}

uintptr_t MouseAlignHook::GetSourcePtr() {
    return (uintptr_t)g_sourcePtr;
}

void MouseAlignHook::SetExternalAccumulatorProvider(
    AccumulatorReader reader, AccumulatorWriter writer) {
    g_externalAccumulatorReader = reader;
    g_externalAccumulatorWriter = writer;
}

bool MouseAlignHook::UsesExternalAccumulatorProvider() {
    return g_externalAccumulatorReader && g_externalAccumulatorWriter;
}

bool MouseAlignHook::SupportsVirtualJoy() {
    return !UsesExternalAccumulatorProvider();
}

bool MouseAlignHook::SourceAvailable() {
    if (UsesExternalAccumulatorProvider()) {
        float yaw{};
        float pitch{};
        return g_externalAccumulatorReader(&yaw, &pitch);
    }
    return GetSourcePtr() != 0;
}

bool MouseAlignHook::ReadAccumulator(float& yaw, float& pitch) {
    if (UsesExternalAccumulatorProvider()) {
        return g_externalAccumulatorReader(&yaw, &pitch);
    }
    const uintptr_t source = GetSourcePtr();
    if (!source) return false;
    __try {
        yaw = *reinterpret_cast<volatile float*>(source + 0x4C);
        pitch = *reinterpret_cast<volatile float*>(source + 0x50);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool MouseAlignHook::WriteAccumulator(float yaw, float pitch) {
    if (UsesExternalAccumulatorProvider()) {
        return g_externalAccumulatorWriter(yaw, pitch);
    }
    const uintptr_t source = GetSourcePtr();
    if (!source) return false;
    __try {
        *reinterpret_cast<volatile float*>(source + 0x4C) = yaw;
        *reinterpret_cast<volatile float*>(source + 0x50) = pitch;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t MouseAlignHook::GetWriterClusterPtr() {
    return (uintptr_t)g_writerClusterPtr;
}

void MouseAlignHook::SetVirtualJoy(bool enabled, float yaw, float pitch) {
    g_vJoyEnabled = enabled ? 1 : 0;
    g_vYaw = yaw;
    g_vPitch = pitch;
    if (g_vRadius > 0.001f) {
        g_vYawNorm = yaw / g_vRadius;
        g_vPitchNorm = pitch / g_vRadius;
    }
}

float MouseAlignHook::GetVirtualYaw() {
    return g_vYaw;
}

float MouseAlignHook::GetVirtualPitch() {
    return g_vPitch;
}
