#pragma once
#include <RE.h>

namespace Offsets {
    // Steam offsets from v1.12.376
    inline uintptr_t MWInputListener_SetCurrentInputType_Offset                                             = 0; // 0x140F6D240;
    inline uintptr_t AddGTAOSpatialFilter_Offset                                                            = 0;
    inline uintptr_t FSSAOShaderParameters_OperatorEquals_Offset                                            = 0;
    inline uintptr_t AddGTAOHorizonSearchIntegratePass_FSSAOShaderParameters_OperatorEquals_Callsite_Offset = 0;

    // TODO Fix up MW5.h to not read references to offsets here...
    inline uintptr_t FRHICommandList_GetNativeDevice_Offset = 0; // 0x0D8D7E0;

    static auto MWInputListener_SetCurrentInputType_Bytes =
            "40 55 53 56 41 57 48 8D AC 24 C8 FE FF FF 48 81 EC 38 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 20 01 00 00 0F B6 DA 48 8B F1 E8 ?? ?? ?? ?? 41 BF 01 00 00 00 84 C0 74 0C 41 3A DF 8B C3 41 0F 45 C7 0F B6 D8";
    static auto AddGTAOSpatialFilter_Bytes =
            "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 38 FE FF FF 48 81 EC 88 02 00 00 0F 29 70 A8 0F 29 78 98 48 ?? ?? ?? ?? ?? ?? 48 33 C4 48 89 85 50 01 00 00";
    static auto FSSAOShaderParameters_OperatorEquals_Bytes =
            "0F 10 02 0F 11 01 0F 10 4A 10 0F 11 49 10 0F 10 42 20 0F 11 41 20 0F 10 4A 30 0F 11 49 30 0F 10 42 40 0F 11 41 40 F2 0F 10 4A 50 F2 0F 11 49 50 F2 0F 10 42 58 F2 0F 11 41 58";
    static auto AddGTAOHorizonSearchIntegratePass_FSSAOShaderParameters_OperatorEquals_Callsite_Bytes =
            "4C 8B 45 B0 48 8D 4D 00 48 8B D3";

    static bool FindAll() {
        const auto text = GetExeTextRange();
        if (!text)
            return false;

        if (!Find(text.value(), "MWInputListener::SetCurrentInputType", MWInputListener_SetCurrentInputType_Bytes, MWInputListener_SetCurrentInputType_Offset))
            return false;

        if (!Find(text.value(), "AddGTAOSpatialFilter", AddGTAOSpatialFilter_Bytes, AddGTAOSpatialFilter_Offset))
            return false;

        if (!Find(text.value(), "FSSAOShaderParameters::operator=", FSSAOShaderParameters_OperatorEquals_Bytes, FSSAOShaderParameters_OperatorEquals_Offset))
            return false;

        if (!Find(text.value(), "AddGTAOHorizonSearchIntegratePass FSSAOShaderParameters::operator= callsite",
                  AddGTAOHorizonSearchIntegratePass_FSSAOShaderParameters_OperatorEquals_Callsite_Bytes,
                  AddGTAOHorizonSearchIntegratePass_FSSAOShaderParameters_OperatorEquals_Callsite_Offset))
            return false;

        return true;
    }
}
