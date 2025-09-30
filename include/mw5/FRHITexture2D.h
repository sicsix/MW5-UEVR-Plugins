#pragma once

struct FRHITexture2D {
    void** VFTable;

    ID3D11Texture2D* GetNativeResource() {
        using FuncType   = ID3D11Texture2D* (*)(FRHITexture2D* self);
        static auto func = (FuncType)VFTable[7];
        return func(this);
    }

    ID3D11ShaderResourceView* GetNativeShaderResourceView() {
        using FuncType   = ID3D11ShaderResourceView* (*)(FRHITexture2D* self);
        static auto func = (FuncType)VFTable[8];
        return func(this);
    }
};
