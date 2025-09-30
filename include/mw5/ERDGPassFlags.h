#pragma once
#include <cstdint>

enum class ERDGPassFlags : uint8_t {
    None            = 0,
    Raster          = 1 << 0,
    Compute         = 1 << 1,
    AsyncCompute    = 1 << 2,
    Copy            = 1 << 3,
    NeverCull       = 1 << 4,
    SkipRenderPass  = 1 << 5,
    UntrackedAccess = 1 << 6,
    Readback        = Copy | NeverCull,
    CommandMask     = Raster | Compute | AsyncCompute | Copy,
    ScopeMask       = NeverCull | UntrackedAccess
};
