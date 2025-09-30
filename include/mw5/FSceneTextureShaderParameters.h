#pragma once

struct alignas(16) FSceneTextureShaderParameters {
    void* SceneTextures;
    void* MobileSceneTextures;
};

static_assert(sizeof(FSceneTextureShaderParameters) == 0x10);
