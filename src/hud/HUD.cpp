#include "HUD.h"
#include "Renderer.h"
#include "Offsets.h"
#include <RE.h>

HUD::HUD() {
    Instance                  = this;
    PluginExtension::Instance = this;
    PluginName                = "HUD";
    PluginVersion             = "2.0.3";
    Renderer                  = new ::Renderer();
}

HUD::~HUD() {
    FWidget3DSceneProxy::GetDynamicMeshElements.Uninstall();
    FWidget3DSceneProxy::CanBeOccluded.Uninstall();
    PostProcessMotionBlur::AddMotionBlurVelocityPass.Uninstall();
    FMotionBlurFilterPS::TRDGLambdaPass::ExecuteImpl.Uninstall();
    FRendererModule::BeginRenderingViewFamily.Uninstall();
    SCanvas::OnPaint.Uninstall();
}

void HUD::OnInitialize() {
    Offsets::FindAll();
    PostProcessMotionBlur::AddMotionBlurVelocityPass.DetourOffset(Offsets::AddMotionBlurVelocityPass_Offset, &PostProcessMotionBlur_AddMotionBlurVelocityPass);
    FMotionBlurFilterPS::TRDGLambdaPass::ExecuteImpl.DetourOffset(Offsets::ExecuteImpl_Offset, &FMotionBlurFilterPS_TRDGLambdaPass_ExecuteImpl);
    FRendererModule::BeginRenderingViewFamily.DetourOffset(Offsets::FRendererModule_BeginRenderingViewFamily_Offset, &FRendererModule_BeginRenderingViewFamily);
    SCanvas::OnPaint.DetourOffset(Offsets::SCanvas_OnPaint_Offset, &SCanvas_OnPaint);
}

int32_t HUD::SCanvas_OnPaint(SCanvas* self,
                             void*    args,
                             void*    allottedGeometry,
                             void*    myCullingRect,
                             void*    outDrawElements,
                             uint32_t layerId,
                             void*    inWidgetStyle,
                             bool     bParentEnabled) {
    if (Instance->InMech[0]) {
        // Just checking the first frame index is sufficient here, a few frames of delay doesn't matter
        // This skips rendering all 2d hud markers, excluding menus etc.
        return layerId;
    }

    // If we are outside the mech we will want these markers, esp for the star map view
    return SCanvas::OnPaint.OriginalFn(self, args, allottedGeometry, myCullingRect, outDrawElements, layerId, inWidgetStyle, bParentEnabled);
}

void HUD::FRendererModule_BeginRenderingViewFamily(FRendererModule* self, FCanvas* canvas, FSceneViewFamily* viewFamily) {
    FRendererModule::BeginRenderingViewFamily.OriginalFn(self, canvas, viewFamily);

    const uint32_t frameIndex = viewFamily->FrameNumber % 3;

    // This is on the game thread, read the current values for this frame here to avoid threading issues from reading on the render thread
    Instance->CurrentlyInMech       = Instance->ValidateMech();
    Instance->DLSSCurrentlyEnabled  = Instance->CurrentlyInMech && Instance->DLSSEnabled ? *Instance->DLSSEnabled : false;
    Instance->InMech[frameIndex]    = Instance->CurrentlyInMech;
    Instance->ZoomLevel[frameIndex] = Instance->CurrentZoomLevel ? *Instance->CurrentZoomLevel : 1.0f;
    Instance->CurrentBrightness     = Instance->Brightness ? *Instance->Brightness : 1.0f;
}

void HUD::Reset() {
    Pawn              = nullptr;
    TargetBoxId       = {};
    Brightness        = nullptr;
    DLSSEnabled       = nullptr;
    CurrentZoomLevel  = nullptr;
    Brightness        = nullptr;
    CurrentBrightness = 1.0f;

    for (auto& w : HUDWidgets) {
        w.ComponentId       = FPrimitiveComponentId{};
        w.Texture           = nullptr;
        w.SRV               = nullptr;
        w.RenderTargetSizeX = 0;
        w.RenderTargetSizeY = 0;
        std::memset(w.MVPs, 0, sizeof(w.MVPs));
        std::memset(w.RequestDraw, 0, sizeof(w.RequestDraw));
    }

    for (auto& vec : MarkerWidgets) {
        vec.clear();
    }

    for (auto& z : ZoomLevel) {
        z = 1.0f;
    }

    for (auto& im : InMech) {
        im = false;
    }

    CurrentlyInMech      = false;
    DLSSCurrentlyEnabled = false;
}

bool HUD::OnNewPawn(API::UObject* activePawn) {
    if (Pawn) {
        RemoveAllEventHooks(true);
        Reset();
    }

    Pawn = activePawn;
    if (!activePawn)
        return false;

    API::UObject* mechView,* mechMesh = nullptr;

    bool success = TryGetProperty(Pawn, L"MechViewComponent", mechView) &&
                   TryGetProperty(Pawn, L"MechMeshComponent", mechMesh);

    if (!success) {
        LogInfo("Failed to get Mech references - probably not in a Mech");
        return false;
    }

    API::UObject* cockpit,* mechCockpit,* torsoCrosshair,* armsCrosshair,* armsTargetCrosshair,* headCrosshair,* targetBox,* hudManager = nullptr;

    success = TryGetProperty(mechMesh, L"FirstPersonCockpitComponent", cockpit) &&
              TryGetProperty(cockpit, L"ChildActor", mechCockpit) &&
              TryGetProperty(mechCockpit, L"VR_TorsoCrosshair", torsoCrosshair) &&
              TryGetProperty(mechCockpit, L"VR_ArmsCrosshair", armsCrosshair) &&
              TryGetProperty(mechCockpit, L"VR_ArmsTargetCrosshair", armsTargetCrosshair) &&
              TryGetProperty(mechCockpit, L"VR_HeadCrosshair", headCrosshair) &&
              TryGetProperty(mechCockpit, L"VR_TargetBox", targetBox) &&
              TryGetProperty(mechCockpit, L"VR_HUDManager", hudManager) &&
              TryGetPropertyStruct(hudManager, L"Brightness", Brightness) &&
              TryGetPropertyStruct(hudManager, L"ZoomLevel", CurrentZoomLevel) &&
              TryGetPropertyStruct(mechCockpit, L"DLSSEnabled", DLSSEnabled);

    if (!success) {
        LogInfo("Failed to get Mech HUD references, retrying next frame...");
        Pawn = nullptr;
        Reset();
        return false;
    }

    success = TryGetWidget3DRenderData("VR_TorsoCrosshair", WidgetType::TorsoCrosshair, torsoCrosshair) &&
              TryGetWidget3DRenderData("VR_ArmsCrosshair", WidgetType::ArmsCrosshair, armsCrosshair) &&
              TryGetWidget3DRenderData("VR_ArmsTargetCrosshair", WidgetType::ArmsTargetCrosshair, armsTargetCrosshair) &&
              TryGetWidget3DRenderData("VR_HeadCrosshair", WidgetType::HeadCrosshair, headCrosshair);

    if (!success) {
        LogInfo("Failed to get Mech HUD data, retrying next frame...");
        Pawn = nullptr;
        Reset();
        return false;
    }

    TargetBoxId = ((UPrimitiveComponent*)targetBox)->ComponentId;

    LogInfo("Mech HUD references acquired");
    return true;
}

bool HUD::TryGetWidget3DRenderData(const char* name, WidgetType type, API::UObject* component, const bool logFailures) {
    auto widget = &HUDWidgets[(int)type];

    widget->Type                   = type;
    const auto* primitiveComponent = (UPrimitiveComponent*)component;
    widget->ComponentId            = primitiveComponent->ComponentId;

    if (!FWidget3DSceneProxy::GetDynamicMeshElements.Installed) {
        auto* proxy = (FWidget3DSceneProxy*)primitiveComponent->SceneProxy;
        if (!proxy) {
            if (logFailures)
                LogError("Failed to get %s Widget3DSceneProxy, retrying next frame...", name);
            return false;
        }
        // Using the first Widget3DSceneProxy we find to detour the GetDynamicMeshElements vfunc globally
        FWidget3DSceneProxy::GetDynamicMeshElements.DetourFromInstance(proxy, &FWidget3DSceneProxy_GetDynamicMeshElements);
        // Also detour CanBeOccluded to always return false to avoid hud markers disappearing when occluded
        FWidget3DSceneProxy::CanBeOccluded.DetourFromInstance(proxy, &FWidget3DSceneProxy_CanBeOccluded);
    }

    return true;
}

// This is called when the Widget3DSceneProxy is asked to render
// We use it to capture the MVP matrices of the widgets and flag them for rendering in our injected post-process pass
// The original function is not called or the widgets will be drawn twice - once normally and once in post-processing
void HUD::FWidget3DSceneProxy_GetDynamicMeshElements(FWidget3DSceneProxy*             self,
                                                     const TArray<const FSceneView*>& views,
                                                     const FSceneViewFamily&          viewFamily,
                                                     uint32_t                         visibilityMap,
                                                     void*                            collector) {
    if (!self || views.Count == 0)
        return;

    if (self->BlendMode == EWidgetBlendMode::Opaque) {
        // This is a screen, render as normal
        FWidget3DSceneProxy::GetDynamicMeshElements.OriginalFn(self, views, viewFamily, visibilityMap, collector);
        return;
    }

    auto&                widgets = Instance->HUDWidgets;
    HUDWidgetRenderData* widget  = nullptr;
    // Find which widget this is - match the render proxy component ID to our stored ones
    for (auto& w : widgets) {
        if (w.ComponentId.PrimIdValue == self->PrimitiveComponentId.PrimIdValue) {
            widget = &w;
            break;
        }
    }

    // Using frameIndex to implement a simple ring buffer to avoid GPU/CPU sync issues
    // Sometimes this can be called multiple times before the post process pass runs so the FrameNumber modulo keeps them in sync
    const uint32_t frameIndex = viewFamily.FrameNumber % 3;

    const auto view         = views.Data[0];
    const bool isMainPass   = views.Count == 2;
    const bool isZoomCamera = views.Count == 1 && view->bIsSceneCapture;

    if (widget) {
        // HUD widget, only render in the main pass
        if (isMainPass)
            ProcessHUDWidget(self, views, frameIndex, widget);
    } else if (isMainPass || isZoomCamera) // Marker widget, only render if it's either the main pass or zoom camera, internal logic will skip as needed
        ProcessMarkerWidget(self, views, isZoomCamera, Instance->ZoomLevel[frameIndex], Instance->TargetBoxId, Instance->MarkerWidgets[frameIndex]);
}

bool HUD::FWidget3DSceneProxy_CanBeOccluded(FWidget3DSceneProxy*) {
    // The widget component has a material that cannot be changed without recreating the component due to unreal bugs
    // Unfortunately the material has depth testing enabled which will cause it to skip rendering if occluded by parts of the cockpit
    // This hook makes the widget component behave as if it has depth testing disabled so it always gets rendered if it passes frustum culling
    return false;
}

void HUD::ProcessHUDWidget(const FWidget3DSceneProxy*       proxy,
                           const TArray<const FSceneView*>& views,
                           const uint32_t                   frameIndex,
                           HUDWidgetRenderData*             widget) {
    widget->Texture = nullptr;
    widget->SRV     = nullptr;

    if (!proxy || !proxy->RenderTarget || !proxy->RenderTarget->Resource || !proxy->RenderTarget->Resource->Texture2DRHI)
        return;

    const ComPtr<ID3D11Texture2D>          tex = proxy->RenderTarget->Resource->Texture2DRHI->GetNativeResource();
    const ComPtr<ID3D11ShaderResourceView> srv = proxy->RenderTarget->Resource->Texture2DRHI->GetNativeShaderResourceView();

    if (!tex || !srv)
        return;

    widget->Texture           = tex;
    widget->SRV               = srv;
    widget->RenderTargetSizeX = proxy->RenderTarget->SizeX;
    widget->RenderTargetSizeY = proxy->RenderTarget->SizeY;


    const auto& m = GetModelMatrix(proxy->LocalToWorld, widget->RenderTargetSizeX, widget->RenderTargetSizeY, 1.0f);

    // Cache the required data for our post-process pass
    for (auto eye = 0; eye < views.Count; eye++) {
        const auto& matrices = views.Data[eye]->ViewMatrices;

        widget->MVPs[frameIndex][eye] = matrices.ProjectionNoAAMatrix *
                                        matrices.TranslatedViewMatrix *
                                        translate(mat4(1), matrices.PreViewTranslation) * m;

        widget->RequestDraw[frameIndex][eye] = true;
    }
}

void HUD::ProcessMarkerWidget(FWidget3DSceneProxy*                 proxy,
                              const TArray<const FSceneView*>&     views,
                              const bool                           isZoomCamera,
                              const float                          zoomLevel,
                              const FPrimitiveComponentId          targetBoxId,
                              std::vector<MarkerWidgetRenderData>& markers) {
    if (zoomLevel > 1.02f && !isZoomCamera || zoomLevel <= 1.02f && isZoomCamera) {
        // If there is no zoom camera, render markers normally
        // If the camera is active and at zoom 1.0 the markers will continue rendering in the main camera
        // Otherwise if the camera is active and zoomed in we skip rendering the normal camera's markers, they will be rendered into the zoom camera overlay instead
        return;
    }

    if (!proxy || !proxy->RenderTarget || !proxy->RenderTarget->Resource || !proxy->RenderTarget->Resource->Texture2DRHI)
        return;

    const ComPtr<ID3D11Texture2D>          tex = proxy->RenderTarget->Resource->Texture2DRHI->GetNativeResource();
    const ComPtr<ID3D11ShaderResourceView> srv = proxy->RenderTarget->Resource->Texture2DRHI->GetNativeShaderResourceView();

    if (!tex || !srv)
        return;

    MarkerWidgetRenderData widget;
    widget.ComponentId       = proxy->PrimitiveComponentId;
    widget.Texture           = tex;
    widget.SRV               = srv;
    widget.RenderTargetSizeX = proxy->RenderTarget->SizeX;
    widget.RenderTargetSizeY = proxy->RenderTarget->SizeY;

    // Ignore zoom level for the target box - scaling this is already handled in blueprints
    const float zoomScale = targetBoxId.PrimIdValue == widget.ComponentId.PrimIdValue ? 1.0f : 1.0f / zoomLevel;

    const auto& m = GetModelMatrix(proxy->LocalToWorld, widget.RenderTargetSizeX, widget.RenderTargetSizeY, zoomScale);
    for (auto eye = 0; eye < views.Count; eye++) {
        const auto& matrices = views.Data[eye]->ViewMatrices;
        widget.MVPs[eye]     = matrices.ProjectionNoAAMatrix *
                               matrices.TranslatedViewMatrix *
                               translate(mat4(1), matrices.PreViewTranslation) * m;
    }

    markers.push_back(widget);
}

mat4 HUD::GetModelMatrix(const mat4& localToWorld, const int32_t sizeX, const int32_t sizeY, const float zoomScale) {
    // This gives the same result as the original function would have done and scales the widget based on scale & size
    const float scaleMult  = (float)sizeX / 2.0f;
    const float scaleRatio = (float)sizeY / (float)sizeX;

    auto scale = vec3{1, 1, 1} * zoomScale;
    scale *= scaleMult;
    scale.y *= scaleRatio;

    auto m = localToWorld;
    m      = rotate(m, glm::half_pi<float>(), vec3(0, 1, 0));
    m      = rotate(m, glm::half_pi<float>(), vec3(0, 0, 1));
    m[0] *= scale.x;
    m[1] *= scale.y;
    m[2] *= scale.z;
    return m;
}

void HUD::PostProcessMotionBlur_AddMotionBlurVelocityPass(FRDGBuilder*,
                                                          const FViewInfo&,
                                                          const FMotionBlurViewports&,
                                                          FRDGTexture*,
                                                          FRDGTexture*,
                                                          FRDGTexture*,
                                                          FRDGTexture**,
                                                          FRDGTexture**) {
    // Skip this, it's part of motion blur which we don't want to use
}

void HUD::FMotionBlurFilterPS_TRDGLambdaPass_ExecuteImpl(FMotionBlurFilterPS::TRDGLambdaPass* self, FRHICommandList*) {
    const auto& lambda       = self->ExecuteLambda;
    const auto  view         = lambda.View;
    const auto& params       = lambda.PixelShaderParameters;
    const auto  pass         = view->StereoPass;
    const auto  frameNumber  = lambda.View->Family->FrameNumber;
    const auto& viewport     = params->Filter.Color;
    const auto  sceneColor   = params->Filter.ColorTexture;
    const auto& renderTarget = params->RenderTargets.Outputs[0].Texture;
    const float zoomLevel    = Instance->ZoomLevel[frameNumber % 3];
    const bool  inMech       = Instance->InMech[frameNumber % 3] && Instance->CurrentlyInMech;
    Instance->Renderer->RenderHUD(Instance->HUDWidgets, Instance->MarkerWidgets, Instance->CurrentBrightness, zoomLevel, pass, frameNumber, viewport, sceneColor, renderTarget,
                                  inMech);
}

bool HUD::ValidateMech() {
    if (!API::get()->param()->vr->is_runtime_ready()) {
        if (Pawn != nullptr) {
            RemoveAllEventHooks(true);
            Reset();
        }
        return false;
    }

    if (const auto activePawn = API::get()->get_local_pawn(0)) {
        if (activePawn != Pawn)
            return OnNewPawn(activePawn);
        return CurrentlyInMech;
    }

    return false;
}

// ReSharper disable once CppInconsistentNaming
std::unique_ptr<HUD> g_plugin{new HUD()}; // NOLINT(misc-use-internal-linkage)
