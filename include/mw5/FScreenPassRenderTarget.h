#pragma once
#include <cstdint>
#include "FScreenPassTexture.h"

struct FScreenPassRenderTarget : FScreenPassTexture {
    uint8_t LoadAction;
};

static_assert(sizeof(FScreenPassRenderTarget) == 0x20);
