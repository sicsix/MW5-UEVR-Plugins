#pragma once
#include <RE.h>
#include "SGameLayerManager.h"

struct UGameViewportClient {
    void**             VFTable;
    int64_t            Pad000[32];
    void*              ViewportOverlayWidget; // 264
    int64_t            Pad001[1];             // Actually part of the weak ref ptr for above ---- added 2 to this and seems to fix it? there's a +16 offset here somehow
    // Actually no, that's garbage. need to work out wtf is happening here, maybe hook elsewhere to get sgamelayermanager
    // OnPaint??? -> seems that OnPaint is getting offset 0xF8 from here which is TWeakPtr<SWindow> Window; on UGameViewportClient
    // SGameLayerManager::OnPaint gets called twice, first time with TWeakPtr<SWindow> Window; from the GameViewporClient in UGameViewportClient::AddViewportWidgetContent
    SGameLayerManager* GameLayerManagerPtr;   // 280



    using FnA = void(__fastcall *)(UGameViewportClient* self, void* viewportContent, int32_t zOrder);
    static inline FunctionHook<FnA> AddViewportWidgetContent{"UGameViewportClient::AddViewportWidgetContent"};
};
