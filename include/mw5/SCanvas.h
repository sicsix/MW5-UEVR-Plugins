#pragma once
#include <RE.h>

struct SCanvas {
    using Fn70 = int32_t(__fastcall *)(SCanvas* self, void* args, void* allottedGeometry, void* myCullingRect, void* outDrawElements, uint32_t layerId, void* inWidgetStyle, bool bParentEnabled);
    static inline InstanceVTableHook<Fn70> OnPaintInstance{"SCanvas::OnPaint Instance", 70, 72}; // Not currently working
    static inline FunctionHook<Fn70> OnPaint{"SCanvas::OnPaint Global"};
};