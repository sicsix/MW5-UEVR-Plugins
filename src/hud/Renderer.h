#pragma once

#include "MW5.h"
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
using namespace glm;

#include "WidgetRenderData.h"

class Renderer {
public:
    Renderer() = default;

    void RenderHUD(HUDWidgetRenderData                         widgets[4],
                   std::vector<MarkerWidgetRenderData>         markerWidgets[3],
                   float                                       brightness,
                   float                                       zoomLevel,
                   EStereoscopicPass                           pass,
                   uint64_t                                    frameNumber,
                   const FScreenPassTextureViewportParameters& viewport,
                   FRDGTexture*                                sceneColor,
                   FRDGTexture*                                renderTarget,
                   bool                                        inMech);

private:
    ComPtr<ID3D11VertexShader>       TexturedQuadVS;
    ComPtr<ID3D11PixelShader>        HUDOverlayPS;
    ComPtr<ID3D11VertexShader>       FullScreenVS;
    ComPtr<ID3D11PixelShader>        PassthroughPS;
    ComPtr<ID3D11InputLayout>        InputLayout;
    ComPtr<ID3D11Buffer>             VertexBuffer;
    ComPtr<ID3D11SamplerState>       Sampler;
    ComPtr<ID3D11BlendState>         BlendStateOverwrite;
    ComPtr<ID3D11BlendState>         BlendStateHUDCompose;
    ComPtr<ID3D11BlendState>         BlendStateHUDOverlay;
    ComPtr<ID3D11DepthStencilState>  DepthStencilState;
    ComPtr<ID3D11RasterizerState>    RasterizerState;
    ComPtr<ID3D11Texture2D>          ZoomCameraMarkerTexture;
    ComPtr<ID3D11RenderTargetView>   ZoomCameraMarkerRTV;
    ComPtr<ID3D11ShaderResourceView> ZoomCameraMarkerSRV;

    struct HUDTarget {
        ComPtr<ID3D11Texture2D>          Tex;
        ComPtr<ID3D11RenderTargetView>   RTV;
        ComPtr<ID3D11ShaderResourceView> SRV;
        UINT                             Width         = 0;
        UINT                             Height        = 0;
        UINT                             sampleCount   = 1;
        UINT                             sampleQuality = 0;

        bool Ensure(ID3D11Device* dvc, ID3D11Texture2D* backbuffer);
    };

    HUDTarget HUDTarget{};

    struct QuadConstants {
        mat4 MVP;
    };

    ComPtr<ID3D11Buffer> QuadConstantsCB;

    struct PSConstants {
        float Brightness;
        float Padding[3];
    };

    ComPtr<ID3D11Buffer> PSConstantsCB;

    struct VSConstants {
        float2 UVScale;
        float2 UVOffset;
    };

    ComPtr<ID3D11Buffer> VSConstantsCB;

    struct Vertex {
        float Px, Py;
        float U,  V;
    };

    struct StateBackup {
        ID3D11DeviceContext*       Context;
        ComPtr<ID3D11VertexShader> VertexShader;
        ComPtr<ID3D11Buffer>       VSConstantBuffer13;

        ComPtr<ID3D11PixelShader>        PixelShader;
        ComPtr<ID3D11SamplerState>       PixelShaderSampler;
        ComPtr<ID3D11ShaderResourceView> PixelShaderSRV;
        ComPtr<ID3D11Buffer>             PixelShaderCB;

        ComPtr<ID3D11RenderTargetView>  RTV;
        ComPtr<ID3D11DepthStencilView>  DSV;
        ComPtr<ID3D11BlendState>        BlendState;
        FLOAT                           BlendFactor[4]{};
        UINT                            SampleMask = 0;
        ComPtr<ID3D11DepthStencilState> DepthStencilState;
        UINT                            StencilRef = 0;

        ComPtr<ID3D11RasterizerState> RS;
        D3D11_VIEWPORT                Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        UINT                          NumViewports = 1;

        ComPtr<ID3D11InputLayout> InputLayout;
        D3D11_PRIMITIVE_TOPOLOGY  Topology{};
        ComPtr<ID3D11Buffer>      VertexBuffer;
        UINT                      VertexBufferStride;
        UINT                      VertexBufferOffset;

        explicit StateBackup(ID3D11DeviceContext* inCtx) : Context(inCtx) {
            Context->VSGetShader(VertexShader.GetAddressOf(), nullptr, nullptr);
            Context->VSGetConstantBuffers(13, 1, VSConstantBuffer13.GetAddressOf());

            Context->PSGetShader(PixelShader.GetAddressOf(), nullptr, nullptr);
            Context->PSGetSamplers(0, 1, PixelShaderSampler.GetAddressOf());
            Context->PSGetShaderResources(0, 1, PixelShaderSRV.GetAddressOf());
            Context->PSGetConstantBuffers(13, 1, PixelShaderCB.GetAddressOf());

            Context->OMGetRenderTargets(1, RTV.GetAddressOf(), DSV.GetAddressOf());
            Context->OMGetDepthStencilState(DepthStencilState.GetAddressOf(), &StencilRef);
            Context->OMGetBlendState(BlendState.GetAddressOf(), BlendFactor, &SampleMask);

            Context->RSGetState(RS.GetAddressOf());
            Context->RSGetViewports(&NumViewports, Viewports);

            Context->IAGetInputLayout(InputLayout.GetAddressOf());
            Context->IAGetPrimitiveTopology(&Topology);
            Context->IAGetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &VertexBufferStride, &VertexBufferOffset);
        }

        ~StateBackup() {
            Context->VSSetShader(VertexShader.Get(), nullptr, 0);
            Context->VSSetConstantBuffers(13, 1, VSConstantBuffer13.GetAddressOf());

            Context->PSSetShader(PixelShader.Get(), nullptr, 0);
            Context->PSSetSamplers(0, 1, PixelShaderSampler.GetAddressOf());
            Context->PSSetShaderResources(0, 1, PixelShaderSRV.GetAddressOf());
            Context->PSSetConstantBuffers(13, 1, PixelShaderCB.GetAddressOf());

            Context->OMSetRenderTargets(1, RTV.GetAddressOf(), DSV.Get());
            Context->OMSetDepthStencilState(DepthStencilState.Get(), StencilRef);
            Context->OMSetBlendState(BlendState.Get(), BlendFactor, SampleMask);

            Context->RSSetState(RS.Get());
            Context->RSSetViewports(NumViewports, Viewports);

            Context->IASetInputLayout(InputLayout.Get());
            Context->IASetPrimitiveTopology(Topology);
            Context->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &VertexBufferStride, &VertexBufferOffset);
        }
    };

    static bool CompileAndCreateVertexShader(ID3D11Device* dvc, const wchar_t* filename, ID3D11VertexShader** vs, ID3DBlob** b);
    static bool CompileAndCreatePixelShader(ID3D11Device* dvc, const wchar_t* filename, ID3D11PixelShader** ps, ID3DBlob** b);
    bool        EnsurePipeline(ID3D11Device* dvc);
    void        RenderMarkersToCurrentRT(ID3D11DeviceContext* ctx, const std::vector<MarkerWidgetRenderData>* markers, int32_t eye) const;
    static void CreateZoomCameraMarkerRT(ID3D11Device*                     dvc,
                                         int32_t                           width,
                                         int32_t                           height,
                                         ComPtr<ID3D11Texture2D>&          texture,
                                         ComPtr<ID3D11RenderTargetView>&   rtv,
                                         ComPtr<ID3D11ShaderResourceView>& srv);
};
