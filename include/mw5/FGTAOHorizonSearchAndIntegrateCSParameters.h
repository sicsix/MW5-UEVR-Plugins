#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

#include "FMotionBlurFilterParameters.h"
#include "FScreenPassTextureViewportTransform.h"

struct FHZBParameters {
    FRDGTexture*                        HZBTexture;
    FRHISamplerState*                   HZBSampler;
    FScreenPassTextureViewportTransform HZBRemapping;
};

static_assert(sizeof(FHZBParameters) == 0x20);

struct FSSAOShaderParameters {
    int32_t                              ScreenSpaceAOParams[20];
    FScreenPassTextureViewportParameters AOViewport;
    FScreenPassTextureViewportParameters AOSceneViewport;
};

static_assert(sizeof(FSSAOShaderParameters) == 0x130);

struct FGTAOShaderParameters {
    int32_t GTAOParams[20];
};

static_assert(sizeof(FGTAOShaderParameters) == 0x50);


struct alignas(16) FGTAOHorizonSearchAndIntegrateCSParameters {
    void*                 View;
    void*                 SceneTextures;
    FHZBParameters        HZBParameters;
    FSSAOShaderParameters SSAOParameters;
    FGTAOShaderParameters GTAOParameters;
    void*                 OutTexture;
};

static_assert(sizeof(FGTAOHorizonSearchAndIntegrateCSParameters) == 0x1C0);
