#pragma once
#include <RE.h>
#include <d3d11.h>
#include "Offsets.h"

struct FRHICommandList {
    void*        Root;
    void**       CommandLink;
    bool         bExecuting;
    unsigned int NumCommands;
    // more members...

    // Flushes the RHICommandList and returns the ID3D11Device - this is always inlined but the implementation at this address does the job
    static void GetNativeDevice(ID3D11Device** outDevice) {
        using FuncType      = void(__fastcall*)(ID3D11Device***);
        ID3D11Device** tmp  = outDevice;
        static auto    func = ResolveOffset<FuncType>(Offsets::FRHICommandList_GetNativeDevice_Offset);
        return func(&tmp);
    }
};
