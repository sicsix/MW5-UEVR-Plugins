#include "Renderer.h"

#include "Log.h"
#include "Util.h"

bool Renderer::HUDTarget::Ensure(ID3D11Device* dvc, ID3D11Texture2D* backbuffer) {
    {
        if (!dvc || !backbuffer)
            return false;

        D3D11_TEXTURE2D_DESC bb{};
        backbuffer->GetDesc(&bb);

        const bool needsRecreate = !Tex || Width != bb.Width || Height != bb.Height;

        if (!needsRecreate)
            return true;

        Tex.Reset();
        RTV.Reset();
        SRV.Reset();

        ComPtr<ID3D11Texture2D>          tex;
        ComPtr<ID3D11RenderTargetView>   rtv;
        ComPtr<ID3D11ShaderResourceView> srv;

        Width         = bb.Width;
        Height        = bb.Height;
        sampleCount   = bb.SampleDesc.Count;
        sampleQuality = bb.SampleDesc.Quality;

        D3D11_TEXTURE2D_DESC td{};
        td.Width              = Width;
        td.Height             = Height;
        td.MipLevels          = 1;
        td.ArraySize          = 1;
        td.Format             = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        td.SampleDesc.Count   = sampleCount;
        td.SampleDesc.Quality = sampleQuality;
        td.Usage              = D3D11_USAGE_DEFAULT;
        td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags     = 0;
        td.MiscFlags          = 0;

        if (FAILED(dvc->CreateTexture2D(&td, nullptr, tex.GetAddressOf())))
            return false;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        rtvDesc.Texture2D.MipSlice = 0;
        if (FAILED(dvc->CreateRenderTargetView(tex.Get(), &rtvDesc, rtv.GetAddressOf())))
            return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format                    = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels       = 1;

        if (FAILED(dvc->CreateShaderResourceView(tex.Get(), &srvDesc, srv.GetAddressOf())))
            return false;

        Tex = tex;
        SRV = srv;
        RTV = rtv;

        Log::LogInfo("HUD render target created: %ux%u", Width, Height);

        return true;
    }
}


bool Renderer::CompileAndCreateVertexShader(ID3D11Device* dvc, const wchar_t* filename, ID3D11VertexShader** vs, ID3DBlob** b) {
    const auto modulePath = Util::GetModulePath();
    const auto path       = modulePath / L"shaders" / filename;
    Log::LogInfo("Compiling Vertex Shader %ls...", path.c_str());

    ComPtr<ID3DBlob> err;

    auto hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "Main", "vs_5_0", 0, 0, b, &err);
    if (FAILED(hr)) {
        if (err)
            Log::LogError("VS compile error: %s", (const char*)err->GetBufferPointer());
        else
            Log::LogError("VS compile error: unknown (HRESULT 0x%08X)", hr);

        return false;
    }

    hr = dvc->CreateVertexShader((*b)->GetBufferPointer(), (*b)->GetBufferSize(), nullptr, vs);
    if (FAILED(hr)) {
        Log::LogError("VS creation error: (HRESULT 0x%08X)", hr);
        return false;
    }

    return true;
}

bool Renderer::CompileAndCreatePixelShader(ID3D11Device* dvc, const wchar_t* filename, ID3D11PixelShader** ps, ID3DBlob** b) {
    const auto modulePath = Util::GetModulePath();
    const auto path       = modulePath / L"shaders" / filename;
    Log::LogInfo("Compiling Pixel Shader %ls...", path.c_str());

    ComPtr<ID3DBlob> err;

    auto hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "Main", "ps_5_0", 0, 0, b, &err);
    if (FAILED(hr)) {
        if (err)
            Log::LogError("PS compile error: %s", (const char*)err->GetBufferPointer());
        else
            Log::LogError("PS compile error: unknown (HRESULT 0x%08X)", hr);

        return false;
    }

    hr = dvc->CreatePixelShader((*b)->GetBufferPointer(), (*b)->GetBufferSize(), nullptr, ps);
    if (FAILED(hr)) {
        Log::LogError("PS creation error: (HRESULT 0x%08X)", hr);
        return false;
    }

    return true;
}

bool Renderer::EnsurePipeline(ID3D11Device* dvc) {
    if (TexturedQuadVS && HUDOverlayPS && PassthroughPS && FullScreenVS && InputLayout && VertexBuffer && Sampler && BlendStateHUDCompose && BlendStateHUDOverlay &&
        BlendStateOverwrite && DepthStencilState && RasterizerState)
        return true;

    constexpr Vertex vertices[6] = {
        {.Px = 1.f, .Py = -1.f, .U = 0.f, .V = 1.f},
        {.Px = 1.f, .Py = 1.f, .U = 0.f, .V = 0.f},
        {.Px = -1.f, .Py = 1.f, .U = 1.f, .V = 0.f},
        {.Px = 1.f, .Py = -1.f, .U = 0.f, .V = 1.f},
        {.Px = -1.f, .Py = 1.f, .U = 1.f, .V = 0.f},
        {.Px = -1.f, .Py = -1.f, .U = 1.f, .V = 1.f},
    };

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    const D3D11_SUBRESOURCE_DATA srd{vertices, 0, 0};
    if (FAILED(dvc->CreateBuffer(&bd, &srd, &VertexBuffer))) {
        return false;
    }

    Log::LogInfo("Compiling shaders...");

    ComPtr<ID3DBlob> vsb, psb, vscb, pscb;

    if (!CompileAndCreateVertexShader(dvc, L"TexturedQuadVS.hlsl", TexturedQuadVS.GetAddressOf(), vsb.GetAddressOf()) ||
        !CompileAndCreatePixelShader(dvc, L"HUDOverlayPS.hlsl", HUDOverlayPS.GetAddressOf(), psb.GetAddressOf()) ||
        !CompileAndCreateVertexShader(dvc, L"FullScreenVS.hlsl", FullScreenVS.GetAddressOf(), vscb.GetAddressOf()) ||
        !CompileAndCreatePixelShader(dvc, L"PassthroughPS.hlsl", PassthroughPS.GetAddressOf(), pscb.GetAddressOf())
    ) {
        Log::LogError("Compiling shaders failed");
        return false;
    }

    Log::LogInfo("Shader compilation successful");

    if (!QuadConstantsCB) {
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth      = sizeof(QuadConstants);
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(dvc->CreateBuffer(&cbd, nullptr, &QuadConstantsCB))) {
            return false;
        }
    }

    if (!PSConstantsCB) {
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth      = sizeof(PSConstants);
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(dvc->CreateBuffer(&cbd, nullptr, &PSConstantsCB))) {
            return false;
        }
    }

    if (!VSConstantsCB) {
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth      = sizeof(VSConstants);
        cbd.Usage          = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(dvc->CreateBuffer(&cbd, nullptr, &VSConstantsCB))) {
            return false;
        }
    }

    constexpr D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(Vertex, Px), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(Vertex, U), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (FAILED(dvc->CreateInputLayout(ied, 2, vsb->GetBufferPointer(), vsb->GetBufferSize(), &InputLayout))) {
        return false;
    }

    D3D11_SAMPLER_DESC sd{};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU       = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD         = 0;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    if (FAILED(dvc->CreateSamplerState(&sd, &Sampler))) {
        return false;
    }
    {
        D3D11_BLEND_DESC bld{};
        bld.IndependentBlendEnable                = FALSE;
        bld.RenderTarget[0].BlendEnable           = FALSE;
        bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(dvc->CreateBlendState(&bld, &BlendStateOverwrite)))
            return false;
    }
    {
        D3D11_BLEND_DESC bld{};
        bld.IndependentBlendEnable                = FALSE;
        bld.RenderTarget[0].BlendEnable           = TRUE;
        bld.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
        bld.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bld.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bld.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bld.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
        bld.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(dvc->CreateBlendState(&bld, &BlendStateHUDOverlay)))
            return false;
    }
    {
        D3D11_BLEND_DESC bld{};
        bld.RenderTarget[0].BlendEnable           = TRUE;
        bld.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bld.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bld.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bld.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bld.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
        bld.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_MAX;
        bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(dvc->CreateBlendState(&bld, &BlendStateHUDCompose))) {
            return false;
        }
    }

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable   = FALSE;
    dsd.StencilEnable = FALSE;
    if (FAILED(dvc->CreateDepthStencilState(&dsd, &DepthStencilState))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(dvc->CreateRasterizerState(&rd, &RasterizerState))) {
        return false;
    }

    return true;
}

void Renderer::RenderHUD(HUDWidgetRenderData                         widgets[4],
                         std::vector<MarkerWidgetRenderData>         markerWidgets[3],
                         float                                       brightness,
                         float                                       zoomLevel,
                         EStereoscopicPass                           pass,
                         uint64_t                                    frameNumber,
                         const FScreenPassTextureViewportParameters& viewport,
                         FRDGTexture*                                sceneColor,
                         FRDGTexture*                                renderTarget,
                         bool                                        inMech) {
    ID3D11Device*        rawDevice = nullptr;
    ComPtr<ID3D11Device> dvc;
    // Flushes command list
    FRHICommandList::GetNativeDevice(&rawDevice);
    dvc = rawDevice;

    if (!dvc) {
        Log::LogError("Failed to get RHI device");
        return;
    }

    ComPtr<ID3D11DeviceContext> ctx = nullptr;
    dvc->GetImmediateContext(ctx.GetAddressOf());
    if (!ctx) {
        Log::LogError("Failed to get context");
        return;
    }

    // Backup existing state, destructor will restore it at the end of the scope
    StateBackup stateBackup(ctx.Get());

    if (!sceneColor || !sceneColor->TextureRHI || !renderTarget || !renderTarget->TextureRHI) {
        Log::LogError("Invalid sceneColor or renderTarget");
        return;
    }

    ID3D11Texture2D* outRT = renderTarget->TextureRHI->GetNativeResource();
    if (!outRT) {
        Log::LogError("Failed to get render target texture");
        return;
    }

    auto* sceneSRV = sceneColor->TextureRHI->GetNativeShaderResourceView();

    if (!sceneSRV) {
        Log::LogError("Failed to get scene color SRV");
        return;
    }

    if (!EnsurePipeline(dvc.Get())) {
        Log::LogError("Failed to build pipeline");
        return;
    }

    if (!HUDTarget.Ensure(dvc.Get(), outRT)) {
        Log::LogError("Failed to build hud target");
        return;
    }

    D3D11_TEXTURE2D_DESC rtDesc{};
    outRT->GetDesc(&rtDesc);

    ComPtr<ID3D11RenderTargetView> outRTV;
    D3D11_RENDER_TARGET_VIEW_DESC  outRTVDesc{};
    outRTVDesc.Format             = rtDesc.Format;
    outRTVDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
    outRTVDesc.Texture2D.MipSlice = 0;
    if (HRESULT hr = dvc->CreateRenderTargetView(outRT, &outRTVDesc, outRTV.GetAddressOf()); FAILED(hr)) {
        Log::LogError("Failed to create RTV for output texture (0x%08X)", hr);
        return;
    }

    const uint32_t frameIndex = frameNumber % 3;

    auto eye = pass == eSSP_RIGHT_EYE || pass == eSSP_RIGHT_EYE_SIDE ? 1 : 0;

    ctx->VSSetShader(TexturedQuadVS.Get(), nullptr, 0);
    ctx->PSSetShader(PassthroughPS.Get(), nullptr, 0);
    ID3D11Buffer* vsCbs[] = {QuadConstantsCB.Get()};
    ctx->VSSetConstantBuffers(13, 1, vsCbs);

    constexpr FLOAT blendFactor[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(BlendStateHUDCompose.Get(), blendFactor, 0xffffffff);
    ctx->OMSetDepthStencilState(DepthStencilState.Get(), 0);
    ctx->RSSetState(RasterizerState.Get());
    ID3D11SamplerState* samplers[] = {Sampler.Get()};
    ctx->PSSetSamplers(0, 1, samplers);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(InputLayout.Get());
    constexpr UINT stride = sizeof(Vertex), offset = 0;
    ID3D11Buffer*  vbs[]  = {VertexBuffer.Get()};
    ctx->IASetVertexBuffers(0, 1, vbs, &stride, &offset);

    auto viewPort     = D3D11_VIEWPORT{};
    viewPort.TopLeftX = (float)viewport.ViewportMin.X;
    viewPort.TopLeftY = (float)viewport.ViewportMin.Y;
    viewPort.Width    = viewport.ViewportSize.X;
    viewPort.Height   = viewport.ViewportSize.Y;
    viewPort.MinDepth = 0.0f;
    viewPort.MaxDepth = 1.0f;

    constexpr FLOAT clearColor[4] = {0, 0, 0, 0};
    ctx->ClearRenderTargetView(HUDTarget.RTV.Get(), clearColor);

    if (inMech) {
        auto& markers = markerWidgets[frameIndex];

        if (zoomLevel > 1.02f) {
            const auto torsoCrosshair = widgets[(int32_t)WidgetType::TorsoCrosshair];

            if (eye == 0) {
                // Validate/create and switch to the 2D camera overlay RT and clear it
                if (torsoCrosshair.RenderTargetSizeX == 0 || torsoCrosshair.RenderTargetSizeY == 0)
                    return;

                if (!ZoomCameraMarkerRTV || !ZoomCameraMarkerSRV) {
                    CreateZoomCameraMarkerRT(dvc.Get(), torsoCrosshair.RenderTargetSizeX, torsoCrosshair.RenderTargetSizeY, ZoomCameraMarkerTexture, ZoomCameraMarkerRTV,
                                             ZoomCameraMarkerSRV);
                    if (!ZoomCameraMarkerRTV || !ZoomCameraMarkerSRV)
                        return;
                }

                auto zoomCamViewport     = D3D11_VIEWPORT{};
                zoomCamViewport.TopLeftX = 0.0f;
                zoomCamViewport.TopLeftY = 0.0f;
                zoomCamViewport.Width    = (float)torsoCrosshair.RenderTargetSizeX;
                zoomCamViewport.Height   = (float)torsoCrosshair.RenderTargetSizeY;
                zoomCamViewport.MinDepth = 0.0f;
                zoomCamViewport.MaxDepth = 1.0f;

                ctx->RSSetViewports(1, &zoomCamViewport);
                ctx->OMSetRenderTargets(1, ZoomCameraMarkerRTV.GetAddressOf(), nullptr);
                ctx->ClearRenderTargetView(ZoomCameraMarkerRTV.Get(), clearColor);
                // Use the passthrough shader, we only want to draw in sRGB without brightness adjustment, this will be done later
                ctx->PSSetShader(PassthroughPS.Get(), nullptr, 0);

                // Render markers to the 2D camera overlay RT - corrective scale has already been applied in the MVPs and the MVPs are from the 2D camera
                RenderMarkersToCurrentRT(ctx.Get(), &markers, 0);
            }

            if (!ZoomCameraMarkerRTV || !ZoomCameraMarkerSRV)
                return;

            // Use the MVP for rendering the TorsoCrosshair widget - it will be the exact same dimensions and location
            ctx->RSSetViewports(1, &viewPort);
            ctx->OMSetRenderTargets(1, HUDTarget.RTV.GetAddressOf(), nullptr);
            ctx->PSSetShader(PassthroughPS.Get(), nullptr, 0);
            auto zoomSrv = ZoomCameraMarkerSRV.Get();
            ctx->PSSetShaderResources(0, 1, &zoomSrv);

            if (D3D11_MAPPED_SUBRESOURCE m; SUCCEEDED(ctx->Map(QuadConstantsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
                auto* vs = (QuadConstants*)m.pData;
                vs->MVP  = torsoCrosshair.MVPs[frameIndex][eye];
                ctx->Unmap(QuadConstantsCB.Get(), 0);
            }

            ctx->Draw(6, 0);

            ID3D11ShaderResourceView* nullSrv[1] = {nullptr};
            ctx->PSSetShaderResources(0, 1, nullSrv);
        } else {
            ctx->RSSetViewports(1, &viewPort);
            ctx->OMSetRenderTargets(1, HUDTarget.RTV.GetAddressOf(), nullptr);
            RenderMarkersToCurrentRT(ctx.Get(), &markers, eye);
        }

        for (int i = 0; i < 4; ++i) {
            auto& widget = widgets[i];

            if (!widget.RequestDraw[frameIndex][eye] || !widget.SRV)
                continue;

            // Log::LogInfo("Rendering HUD widget %d for eye %d", i, eye);

            ctx->PSSetShaderResources(0, 1, widget.SRV.GetAddressOf());

            if (D3D11_MAPPED_SUBRESOURCE m; SUCCEEDED(ctx->Map(QuadConstantsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
                auto* vs = (QuadConstants*)m.pData;
                vs->MVP  = widget.MVPs[frameIndex][eye];
                ctx->Unmap(QuadConstantsCB.Get(), 0);
            }

            ctx->Draw(6, 0);

            ID3D11ShaderResourceView* nullSrv[1] = {nullptr};
            ctx->PSSetShaderResources(0, 1, nullSrv);

            widget.RequestDraw[frameIndex][eye] = false;
        }

        // Log::LogInfo("Eye %d HUD rendered with %zu markers", eye, markers.size());

        // Clear markers after both eyes have rendered or if rendering a non-stereo frame
        if (eye == 1 || pass == eSSP_FULL)
            markers.clear();
    }

    ctx->OMSetRenderTargets(1, outRTV.GetAddressOf(), nullptr);
    ID3D11SamplerState* samps[] = {Sampler.Get()};
    ctx->PSSetSamplers(0, 1, samps);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(nullptr);
    ctx->VSSetShader(FullScreenVS.Get(), nullptr, 0);
    ctx->RSSetViewports(1, &viewPort);

    float2 uvScale  = {viewPort.Width / ((float)rtDesc.Width * 0.5f), 1.0f};
    float2 uvOffset = {viewPort.TopLeftX != 0.0f ? 0.5f : 0.0f, 0.0f};

    ID3D11Buffer* vsCBs[] = {VSConstantsCB.Get()};
    ctx->VSSetConstantBuffers(13, 1, vsCBs);

    ID3D11Buffer* psCBs[] = {PSConstantsCB.Get()};
    ctx->PSSetConstantBuffers(13, 1, psCBs);

    if (D3D11_MAPPED_SUBRESOURCE m; SUCCEEDED(ctx->Map(VSConstantsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        auto* vs     = (VSConstants*)m.pData;
        vs->UVOffset = uvOffset;
        vs->UVScale  = uvScale;
        ctx->Unmap(VSConstantsCB.Get(), 0);
    }

    if (D3D11_MAPPED_SUBRESOURCE m; SUCCEEDED(ctx->Map(PSConstantsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        auto* ps       = (PSConstants*)m.pData;
        ps->Brightness = brightness;
        ctx->Unmap(PSConstantsCB.Get(), 0);
    }

    // Copy scene color to the render target
    ctx->PSSetShaderResources(0, 1, &sceneSRV);
    ctx->OMSetBlendState(BlendStateOverwrite.Get(), blendFactor, 0xffffffff);
    ctx->PSSetShader(PassthroughPS.Get(), nullptr, 0);
    ctx->Draw(3, 0);

    if (inMech) {
        // Copy the HUD to the render target, applying brightness and using a mixed alpha additive blend
        ctx->PSSetShaderResources(0, 1, HUDTarget.SRV.GetAddressOf());
        ctx->OMSetBlendState(BlendStateHUDOverlay.Get(), blendFactor, 0xffffffff);
        ctx->PSSetShader(HUDOverlayPS.Get(), nullptr, 0);
        ctx->Draw(3, 0);
    }
}

void Renderer::RenderMarkersToCurrentRT(ID3D11DeviceContext* ctx, const std::vector<MarkerWidgetRenderData>* markers, const int32_t eye) const {
    for (auto& marker : *markers) {
        if (!marker.SRV)
            continue;

        ctx->PSSetShaderResources(0, 1, marker.SRV.GetAddressOf());

        if (D3D11_MAPPED_SUBRESOURCE m; SUCCEEDED(ctx->Map(QuadConstantsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
            auto* vs = (QuadConstants*)m.pData;
            vs->MVP  = marker.MVPs[eye];
            ctx->Unmap(QuadConstantsCB.Get(), 0);
        }

        ctx->Draw(6, 0);

        ID3D11ShaderResourceView* nullSrv[1] = {nullptr};
        ctx->PSSetShaderResources(0, 1, nullSrv);
    }
}

void Renderer::CreateZoomCameraMarkerRT(ID3D11Device*                     dvc,
                                        const int32_t                     width,
                                        const int32_t                     height,
                                        ComPtr<ID3D11Texture2D>&          texture,
                                        ComPtr<ID3D11RenderTargetView>&   rtv,
                                        ComPtr<ID3D11ShaderResourceView>& srv) {
    texture.Reset();
    rtv.Reset();
    srv.Reset();

    ComPtr<ID3D11Texture2D>          localTex;
    ComPtr<ID3D11RenderTargetView>   localRTV;
    ComPtr<ID3D11ShaderResourceView> localSRV;

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width            = width;
    texDesc.Height           = height;
    texDesc.MipLevels        = 1;
    texDesc.ArraySize        = 1;
    texDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage            = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags   = 0;

    if (FAILED(dvc->CreateTexture2D(&texDesc, nullptr, localTex.GetAddressOf()))) {
        Log::LogError("Failed to create Zoom Camera Marker texture");
        return;
    }

    if (FAILED(dvc->CreateRenderTargetView(localTex.Get(), nullptr, localRTV.GetAddressOf()))) {
        Log::LogError("Failed to create Zoom Camera Marker RTV");
        return;
    }

    if (FAILED(dvc->CreateShaderResourceView(localTex.Get(), nullptr, localSRV.GetAddressOf()))) {
        Log::LogError("Failed to create Zoom Camera Marker SRV");
        return;
    }

    texture = localTex;
    rtv     = localRTV;
    srv     = localSRV;
}
