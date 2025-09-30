#pragma once
#include <cstdint>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

#include "EStereoscopicPass.h"
#include "FIntRect.h"
#include "FViewMatrices.h"

struct FSceneViewFamily;
struct FViewElementDrawer;

struct FSceneView {
    const FSceneViewFamily* Family;
    uint64_t                Pad000[72];
    int32_t                 PlayerIndex;
    FViewElementDrawer*     Drawer;
    FIntRect                UnscaledViewRect;
    FIntRect                UnconstrainedViewRect;
    uint64_t                Pad001[1];
    FViewMatrices           ViewMatrices;
    uint64_t                Pad002[148];
    EStereoscopicPass       StereoPass; // 2768
    float                   StereoIPD;
    bool                    bAllowCrossGPUTransfer;
    bool                    bOverrideGPUMask;
    uint32_t                GPUMask;
    bool                    bRenderFirstInstanceOnly;
    bool                    bUseFieldOfViewForLOD;
    float                   FOV;
    float                   DesiredFOV;
    int32_t                 DrawDynamicFlags;
    uint64_t                Pad003[6];
    float4                  DiffuseOverrideParameter;
    float4                  SpecularOverrideParameter;
    float4                  NormalOverrideParameter;
    FVector2D               RoughnessOverrideParameter;
    float                   MaterialTextureMipBias;
    uint64_t                Pad004[21];
    bool                    bAllowTemporalJitter;
    uint64_t                Pad005[36];
    bool                    bHasNearClippingPlane;
    uint64_t                Pad006[2];
    float                   NearClippingDistance;
    bool                    bReverseCulling;
    float4                  InvDeviceZToWorldZTransform;
    float3                  OriginOffsetThisFrame;
    float                   LODDistanceFactor;
    bool                    bCameraCut;
    FIntPoint               CursorPos;
    bool                    bIsGameView;
    bool                    bIsViewInfo;
    bool                    bIsSceneCapture;
    bool                    bSceneCaptureUsesRayTracing;
    bool                    bIsReflectionCapture;
    bool                    bIsPlanarReflection;
    bool                    bIsVirtualTexture;
    bool                    bIsOfflineRender;
    bool                    bRenderSceneTwoSided;
    bool                    bIsLocked;
    bool                    bStaticSceneOnly;
    bool                    bIsInstancedStereoEnabled;
    bool                    bIsMultiViewEnabled;
    bool                    bIsMobileMultiViewEnabled;
    bool                    bShouldBindInstancedViewUB;
    float                   UnderwaterDepth;
    bool                    bForceCameraVisibilityReset;
};
