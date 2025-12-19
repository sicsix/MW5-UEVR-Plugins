#pragma once
#include <cstdint>
#include <MinHook.h>
#include <Log.h>
#include <psapi.h>
#include <Sig.hpp>

template <typename T>
static T ResolveOffset(const uint64_t offset) {
    const auto base = (uintptr_t)GetModuleHandleW(nullptr);
    auto       addr = (void*)(base + offset);
    Log::LogInfo("[ResolveOffset] Base: %p, Offset: 0x%llX -> Address: %p", (void*)base, offset, addr);
    return (T)addr;
}

struct TextRange {
    const uint8_t* Begin{};
    size_t         Size{};
    HMODULE        Module{};
};

static std::optional<TextRange> GetExeTextRange() {
    const HMODULE mod = GetModuleHandleW(nullptr);
    if (!mod)
        return std::nullopt;

    const auto dos = (const IMAGE_DOS_HEADER*)mod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return std::nullopt;

    auto nt = (const IMAGE_NT_HEADERS*)((const uint8_t*)mod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return std::nullopt;

    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (sec->Characteristics & IMAGE_SCN_CNT_CODE) {
            const uint8_t* start = reinterpret_cast<const uint8_t*>(mod) + sec->VirtualAddress;
            size_t         sz    = sec->Misc.VirtualSize;
            return TextRange{start, sz, mod};
        }
    }

    MODULEINFO mi{};
    if (GetModuleInformation(GetCurrentProcess(), mod, &mi, sizeof(mi))) {
        return TextRange{(const uint8_t*)mi.lpBaseOfDll, mi.SizeOfImage, mod};
    }
    Log::LogError("[Offsets] Failed to get .text section");
    return std::nullopt;
}

static bool Find(const TextRange& text, const std::string& name, const char* pattern, uintptr_t& outRva) {
    const void* hit = Sig::find(text.Begin, text.Size, pattern);

    if (!hit) {
        Log::LogError("[Offsets] Failed to find %s", name.c_str());
        return false;
    }

    outRva = (uintptr_t)hit - (uintptr_t)text.Module;
    Log::LogInfo("[Offsets] %s offset: 0x%p", name.c_str(), (const void*)outRva);
    return true;
}

template <typename Fn>
struct VTableHook {
    const char* Name;
    void**      VTable = nullptr;
    size_t      Index;
    Fn          OriginalFn = nullptr;
    bool        Installed  = false;

    explicit VTableHook(const char* name, const size_t index) : Name(name), Index(index) {}

    bool DetourFromVTable(void** vTable, Fn detour) {
        if (Installed || !vTable || !detour) {
            Log::LogError("[VTableDetour] Invalid parameters for %s", Name);
            return false;
        }

        DWORD oldProt{};
        if (!VirtualProtect(&vTable[Index], sizeof(void*), PAGE_READWRITE, &oldProt)) {
            Log::LogError("[VTableDetour] Failed to change vtable protection for %s", Name);
            return false;
        }

        VTable        = vTable;
        OriginalFn    = (Fn)vTable[Index];
        vTable[Index] = detour;
        VirtualProtect(&vTable[Index], sizeof(void*), oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), &vTable[Index], sizeof(void*));

        Installed = true;
        Log::LogInfo("[VTableDetour] Detoured %s", Name);
        return true;
    }

    bool DetourFromInstance(void* instance, Fn detour) {
        if (Installed || !instance || !detour) {
            Log::LogError("[VTableDetour] Invalid parameters for %s", Name);
            return false;
        }
        return DetourFromVTable(*(void***)instance, detour);
    }

    bool DetourFromOffset(const uintptr_t offset, Fn detour) {
        if (Installed || !offset || !detour) {
            Log::LogError("[VTableDetour] Invalid parameters for %s", Name);
            return false;
        }
        const auto vTable = ResolveOffset<void**>(offset);
        return DetourFromVTable(vTable, detour);
    }

    void Uninstall() {
        if (!Installed || !VTable || !OriginalFn) {
            Log::LogWarn("[VTableDetour] Unable to uninstall %s", Name);
            return;
        }

        DWORD oldProt{};
        if (!VirtualProtect(&VTable[Index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
            Log::LogError("[VTableDetour] Failed to change vtable protection for %s", Name);
            return;
        }

        VTable[Index] = (void*)OriginalFn;
        VirtualProtect(&VTable[Index], sizeof(void*), oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), &VTable[Index], sizeof(void*));

        OriginalFn = nullptr;
        VTable     = nullptr;
        Installed  = false;
        Log::LogInfo("[VTableDetour] Uninstalled %s", Name);
    }
};

template <typename Fn>
struct FunctionHook {
    const char* Name;
    void*       Target     = nullptr;
    Fn          OriginalFn = nullptr;
    bool        Installed  = false;

    explicit FunctionHook(const char* name) : Name(name) {}

    bool DetourOffset(const uintptr_t offset, Fn detour) {
        if (Installed || !offset || !detour) {
            Log::LogError("[FunctionHook] Invalid parameters for %s", Name);
            return false;
        }

        auto target = ResolveOffset<void*>(offset);

        if (MH_CreateHook(target, detour, (void**)&OriginalFn) != MH_OK) {
            Log::LogError("[FunctionHook] Failed to create hook for %s", Name);
            return false;
        }

        if (MH_EnableHook(target) != MH_OK) {
            Log::LogError("[FunctionHook] Failed to enable hook for %s", Name);
            return false;
        }

        Target    = target;
        Installed = true;
        Log::LogInfo("[FunctionHook] Detoured %s at offset 0x%llX", Name, offset);
        return true;
    }

    void Uninstall() {
        if (!Installed || !Target) {
            Log::LogWarn("[FunctionHook] Unable to uninstall %s", Name);
            return;
        }

        if (MH_DisableHook(Target) != MH_OK) {
            Log::LogError("[FunctionHook] Failed to disable hook for %s", Name);
            return;
        }

        Target     = nullptr;
        OriginalFn = nullptr;
        Installed  = false;
        Log::LogInfo("[FunctionHook] Uninstalled %s", Name);
    }
};

template <typename Fn>
struct InstanceVTableHook {
    const char* Name;
    void*       Instance       = nullptr;
    void**      OriginalVTable = nullptr;
    void**      ClonedVTable   = nullptr;
    size_t      VTableCount    = 0;
    size_t      Index;
    Fn          OriginalFn = nullptr;
    bool        Installed  = false;

    explicit InstanceVTableHook(const char* name, const size_t index, const size_t vtableCount) : Name(name), VTableCount(vtableCount), Index(index) {}

    bool DetourInstance(void* instance, Fn detour) {
        if (Installed || !instance || !detour) {
            Log::LogError("[InstanceVTableHook] Invalid parameters for %s", Name);
            return false;
        }

        // Read the current vtable pointer from the instance
        auto vptr = *(void***)instance;
        if (!vptr) {
            Log::LogError("[InstanceVTableHook] Instance has null vtable for %s", Name);
            return false;
        }
        if (Index >= VTableCount) {
            Log::LogError("[InstanceVTableHook] Index %zu out of range (%zu) for %s", Index, VTableCount, Name);
            return false;
        }

        const SIZE_T bytes = VTableCount * sizeof(void*);
        auto         clone = (void**)VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!clone) {
            Log::LogError("[InstanceVTableHook] Failed to allocate vtable clone for %s", Name);
            return false;
        }

        memcpy(clone, vptr, bytes);

        OriginalFn   = (Fn)clone[Index];
        clone[Index] = (void*)detour;

        *(void***)instance = clone;

        Instance       = instance;
        OriginalVTable = vptr;
        ClonedVTable   = clone;
        Installed      = true;

        Log::LogInfo("[InstanceVTableHook] Detoured %s at slot %zu for instance %p (clone %p, original vtbl %p, entries %zu)", Name, Index, Instance, ClonedVTable, OriginalVTable,
                     VTableCount);
        return true;
    }

    void Uninstall() {
        if (!Installed) {
            Log::LogWarn("[InstanceVTableHook] Unable to uninstall %s (not installed)", Name);
            return;
        }

        if (Instance && OriginalVTable)
            *(void***)Instance = OriginalVTable;

        if (ClonedVTable) {
            VirtualFree(ClonedVTable, 0, MEM_RELEASE);
        }

        Log::LogInfo("[InstanceVTableHook] Uninstalled %s (instance %p restored to vtbl %p)", Name, Instance, OriginalVTable);

        Instance       = nullptr;
        OriginalVTable = nullptr;
        ClonedVTable   = nullptr;
        VTableCount    = 0;
        OriginalFn     = nullptr;
        Installed      = false;
    }
};

struct MemoryPatch {
    const char*          Name;
    void*                Address = nullptr;
    size_t               Size    = 0;
    std::vector<uint8_t> OriginalBytes;
    bool                 Installed = false;

    explicit MemoryPatch(const char* name) : Name(name) {}

    bool InstallAtAddress(void* addr, const void* patchData, size_t patchSize) {
        if (Installed || !addr || !patchData || patchSize == 0) {
            Log::LogError("[MemoryPatch] Invalid params for %s (installed=%d, addr=%p, data=%p, size=%zu)", Name, (int)Installed, addr, patchData, patchSize);
            return false;
        }

        DWORD oldProt{};
        if (!VirtualProtect(addr, patchSize, PAGE_EXECUTE_READWRITE, &oldProt)) {
            Log::LogError("[MemoryPatch] VirtualProtect RWX failed for %s at %p (size %zu)", Name, addr, patchSize);
            return false;
        }

        OriginalBytes.resize(patchSize);
        std::memcpy(OriginalBytes.data(), addr, patchSize);

        std::memcpy(addr, patchData, patchSize);

        FlushInstructionCache(GetCurrentProcess(), addr, patchSize);
        DWORD tmp{};
        VirtualProtect(addr, patchSize, oldProt, &tmp);

        Address   = addr;
        Size      = patchSize;
        Installed = true;

        Log::LogInfo("[MemoryPatch] Installed %s at %p (size %zu)", Name, Address, Size);
        return true;
    }

    bool InstallAtOffset(uintptr_t offset, const void* patchData, size_t patchSize) {
        if (!offset) {
            Log::LogError("[MemoryPatch] Offset is 0 for %s", Name);
            return false;
        }
        void* addr = ResolveOffset<void*>(offset);
        return InstallAtAddress(addr, patchData, patchSize);
    }

    bool NopFillAtOffset(uintptr_t offset, size_t patchSize) {
        if (!offset) {
            Log::LogError("[MemoryPatch] Offset is 0 for %s", Name);
            return false;
        }
        void*                addr = ResolveOffset<void*>(offset);
        std::vector<uint8_t> nops(patchSize, 0x90);
        return InstallAtAddress(addr, nops.data(), patchSize);
    }

    bool SkipFromTo(const uintptr_t startAddr, const uintptr_t offsetFrom, const uintptr_t offsetTo) {
        auto*             src    = ResolveOffset<uint8_t*>(startAddr + offsetFrom);
        const auto*       dst    = ResolveOffset<uint8_t*>(startAddr + offsetTo);
        constexpr uint8_t opcode = 0xE9;
        const int64_t     rel64  = dst - (src + 5);
        const int32_t     rel32  = (int32_t)rel64;
        if ((int64_t)rel32 != rel64) {
            Log::LogError("[SkipPatch] Target too far for rel32 jump in %s", "");
            return false;
        }

        constexpr uint32_t   minByteCount = 5;
        std::vector<uint8_t> bytes;
        bytes.reserve(minByteCount);
        bytes.push_back(opcode);
        // little-endian rel32
        bytes.push_back((uint8_t)(rel32 & 0xFF));
        bytes.push_back((uint8_t)((rel32 >> 8) & 0xFF));
        bytes.push_back((uint8_t)((rel32 >> 16) & 0xFF));
        bytes.push_back((uint8_t)((rel32 >> 24) & 0xFF));
        while (bytes.size() < minByteCount)
            bytes.push_back(0x90);

        return InstallAtAddress(src, bytes.data(), bytes.size());
    }

    void Uninstall() {
        if (!Installed || !Address || OriginalBytes.empty() || Size == 0) {
            Log::LogWarn("[MemoryPatch] Unable to restore %s (not installed or invalid state)", Name);
            return;
        }

        DWORD oldProt{};
        if (!VirtualProtect(Address, Size, PAGE_EXECUTE_READWRITE, &oldProt)) {
            Log::LogError("[MemoryPatch] VirtualProtect RWX failed during restore for %s at %p (size %zu)",
                          Name, Address, Size);
            return;
        }

        std::memcpy(Address, OriginalBytes.data(), Size);
        FlushInstructionCache(GetCurrentProcess(), Address, Size);

        DWORD tmp{};
        VirtualProtect(Address, Size, oldProt, &tmp);

        Log::LogInfo("[MemoryPatch] Restored %s at %p (size %zu)", Name, Address, Size);

        Address = nullptr;
        Size    = 0;
        OriginalBytes.clear();
        OriginalBytes.shrink_to_fit();
        Installed = false;
    }
};
