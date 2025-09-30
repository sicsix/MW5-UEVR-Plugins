#pragma once
#include <RE.h>

struct SGameLayerManager {
    using FnA = void*(__fastcall *)(SGameLayerManager* self, void* result, void* localPlayer);
    static inline FunctionHook<FnA> FindOrCreatePlayerLayer{"SGameLayerManager::FindOrCreatePlayerLayer"};
};
