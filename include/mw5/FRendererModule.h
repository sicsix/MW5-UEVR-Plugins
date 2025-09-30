#pragma once
#include <RE.h>
#include "FSceneViewFamily.h"

struct FCanvas;

struct FRendererModule {
    using FnA = void(__fastcall *)(FRendererModule* self, FCanvas* canvas, FSceneViewFamily* viewFamily);
    static inline FunctionHook<FnA> BeginRenderingViewFamily{"SGameLayerManager::FindOrCreatePlayerLayer"};
};