#pragma once
#include <cstdint>
#include "FDepthStencilBinding.h"
#include "FResolveRect.h"
#include "FRenderTargetBinding.h"

struct alignas(16) FRenderTargetBindingSlots {
    FRenderTargetBinding Outputs[8];
    FDepthStencilBinding DepthStencil;
    FResolveRect         ResolveRect;
    uint32_t             NumOcclusionQueries;
};

static_assert(sizeof(FRenderTargetBindingSlots) == 0xF0);
