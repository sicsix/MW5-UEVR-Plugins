#pragma once
#include <RE.h>
#include "SGameLayerManager.h"

struct UGameViewportClient {
    void**             VFTable;
    int64_t            Pad000[32];
    void*              ViewportOverlayWidget; // 264
    int64_t            Pad001[1];
    SGameLayerManager* GameLayerManagerPtr;   // 280

    using FnA = void(__fastcall *)(UGameViewportClient* self, void* viewportContent, int32_t zOrder);
    static inline FunctionHook<FnA> AddViewportWidgetContent{"UGameViewportClient::AddViewportWidgetContent"};
};
