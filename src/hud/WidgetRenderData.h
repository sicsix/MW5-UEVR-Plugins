#pragma once
#include <d3d11.h>
#include <MW5.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

enum class WidgetType : uint8_t {
    TorsoCrosshair,
    ArmsCrosshair,
    ArmsTargetCrosshair,
    HeadCrosshair,
    Count
};

struct HUDWidgetRenderData {
    FPrimitiveComponentId            ComponentId{};
    ComPtr<ID3D11Texture2D>          Texture;
    ComPtr<ID3D11ShaderResourceView> SRV;
    int32_t                          RenderTargetSizeX = 0;
    int32_t                          RenderTargetSizeY = 0;
    mat4                             MVPs[3][2]{};
    bool                             RequestDraw[3][2]{};
    WidgetType                       Type;
};

struct MarkerWidgetRenderData {
    FPrimitiveComponentId            ComponentId{};
    ComPtr<ID3D11Texture2D>          Texture;
    ComPtr<ID3D11ShaderResourceView> SRV;
    int32_t                          RenderTargetSizeX = 0;
    int32_t                          RenderTargetSizeY = 0;
    mat4                             MVPs[2]{};
};
