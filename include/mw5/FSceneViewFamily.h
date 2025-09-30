#pragma once
#include <cstdint>
#include "TArray.h"

struct FSceneView;

struct FSceneViewFamily {
    uint64_t            Pad000[1];
    TArray<FSceneView*> Views{};  // 8
    uint32_t            ViewMode; // 24
    const void*         RenderTarget;
    void*               Scene;
    uint64_t            Pad001[6];
    uint32_t            Pad002[1];
    uint32_t            FrameNumber; // 100
    bool                bAdditionalViewFamily;
    bool                bRealTimeUpdate;
    bool                bDeferClear;
    bool                bResolveScene;
    bool                bMultiGPUForkAndJoin;
    uint32_t            SceneCaptureSource;
    uint32_t            SceneCaptureCompositeMode;
    bool                bWorldIsPaused;
    bool                bIsHDR;
    bool                bRequireMultiView;
    float               GammaCorrection;
    uint64_t            Pad003[4];
    uint32_t            Pad004[1];
    float               SecondaryViewFraction; // 164
    uint32_t            SecondaryScreenPercentageMethod;
};
