#pragma once
#include <RE.h>

namespace Offsets {
    // Steam offsets from v1.12.376
    inline uintptr_t AddMotionBlurVelocityPass_Offset                = 0; // 0x1C2A040;
    inline uintptr_t ExecuteImpl_Offset                              = 0; // 0x1C37F40;
    inline uintptr_t FWidget3DSceneProxy_CanBeOccluded_Offset        = 0; // 0x25FE1F0;
    inline uintptr_t FRHICommandList_GetNativeDevice_Offset          = 0; // 0x0D8D7E0;
    inline uintptr_t FRendererModule_BeginRenderingViewFamily_Offset = 0; // 0x1CF3260;
    inline uintptr_t SCanvas_OnPaint_Offset                          = 0; // 0x186D670;

    static auto AddMotionBlurVelocityPass_Bytes =
            "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 54 41 55 41 56 41 57 48 8D A8 18 FE FF FF 48 81 EC C0 02 00 00 0F 29 70 C8";
    static auto AddMotionBlurFilterPass_Bytes =
            "48 89 5C 24 18 55 56 57 41 55 41 57 48 8D AC 24 50 FE FF FF 48 81 EC B0 02 00 00";
    static auto AddPass_MotionBlurFilterPS_Bytes = // Steam 0x1C0CA80
            "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 30 41 0F B6 C1 49 8B F0 4C 8B F2 44 0F B6 C0 45 33 C9 48 ?? ?? ?? ?? ?? ?? 48 8B E9 E8 ?? ?? ?? ?? 45 33 C9 48 8D 55 10 41 B8 01 00 00 00 B9 88 01 00 00 44 0F B6 F8 E8 ?? ?? ?? ?? 48 8B F8 48 85 C0 74 29 48 8B 54 24 70 48 ?? ?? ?? ?? ?? ?? 48 89 54 24 20 48 8D 4F 08 49 8B D6 48 89 07 45 0F B6 CF 4C 8B C6 E8 AE 42 01 00 EB 02";
    static auto FWidget3DSceneProxy_CanBeOccluded_Bytes =
            "0F B6 81 94 01 00 00 C0 E8 03 F6 D0 24 01 C3";
    static auto FRHICommandList_GetNativeDevice_Bytes =
            "48 89 5C 24 08 57 48 83 EC 20 8B 1D ?? ?? ?? ?? 48 8B F9 8B D3 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 ?? ?? ?? ?? ?? ?? 76 13 48 8D 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8";
    static auto FRendererModule_BeginRenderingViewFamily_Bytes =
            "48 89 6C 24 10 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 60 49 8B 48";
    static auto SCanvas_OnPaint_Bytes =
            "4C 8B DC 41 56 48 81 EC F0 01 00 00 48 8B ?? ?? ?? ?? ?? 48 33 C4 48 89 84 24 B8 01 00 00 48 8B 84 24 30 02 00 00 4D 8B D0";

    static bool FindAll() {
        const auto text = GetExeTextRange();
        if (!text) {
            Log::LogError("[Offsets] Failed to get .text section");
            return false;
        }

        if (!Find(text.value(), "AddMotionBlurVelocityPass", AddMotionBlurVelocityPass_Bytes, AddMotionBlurVelocityPass_Offset))
            return false;

        if (!Find(text.value(), "FWidget3DSceneProxy::CanBeOccluded", FWidget3DSceneProxy_CanBeOccluded_Bytes, FWidget3DSceneProxy_CanBeOccluded_Offset))
            return false;

        if (!Find(text.value(), "FRHICommandList::GetNativeDevice", FRHICommandList_GetNativeDevice_Bytes, FRHICommandList_GetNativeDevice_Offset))
            return false;

        if (!Find(text.value(), "FRendererModule::BeginRenderingViewFamily", FRendererModule_BeginRenderingViewFamily_Bytes, FRendererModule_BeginRenderingViewFamily_Offset))
            return false;

        if (!Find(text.value(), "SCanvas::OnPaint", SCanvas_OnPaint_Bytes, SCanvas_OnPaint_Offset))
            return false;

        // This convoluted mess is to find the ExecuteImpl of the FMotionBlurFilterPS lambda pass, unfortunately everything leading there has too many hits
        // This may prove too brittle
        uintptr_t rva;
        if (!Find(text.value(), "AddPass_FMotionBlurFilterPS", AddPass_MotionBlurFilterPS_Bytes, rva))
            return false;

        uintptr_t address = (uintptr_t)text->Module + rva;
        Log::LogInfo("[Offsets] AddPass_FMotionBlurFilterPS address: 0x%p", (const void*)address);
        // Offset TRDGLambdaPass_FMotionBlurFilterPS_ctor
        address += 0x7D;
        if (((const uint8_t*)address)[0] != 0xE8) {
            Log::LogError("[Offsets] AddPass_FMotionBlurFilterPS: expected call instruction");
            return false;
        }

        // Move to called function
        address += 1;
        int32_t rel = *(int32_t*)address;
        address += 4;
        address += rel;
        Log::LogInfo("[Offsets] FMotionBlurFilterPS__TRDGLambdaPass_ctor address: 0x%p", (const void*)address);

        // Offset to lea rax, off_14XXXXXXXX
        address += 0x50;
        if (((const uint8_t*)address)[0] != 0x48 || ((const uint8_t*)address)[1] != 0x8D || ((const uint8_t*)address)[2] != 0x05) {
            Log::LogError("[Offsets] AddPass_FMotionBlurFilterPS: expected lea instruction");
            return false;
        }

        // Move to offset
        address += 3;
        rel = *(int32_t*)address;
        address += 4;
        address += rel;
        Log::LogInfo("[Offsets] FMotionBlurFilterPS__TRDGLambdaPass vtable address: 0x%p", (const void*)address);

        // Get second vtable entry, which is ExecuteImpl
        const auto vtable  = (uintptr_t*)address;
        ExecuteImpl_Offset = vtable[1] - (uintptr_t)text->Module;
        Log::LogInfo("[Offsets] ExecuteImpl offset: 0x%p", (const void*)ExecuteImpl_Offset);

        return true;
    }
}
