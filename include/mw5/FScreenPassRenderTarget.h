#pragma once
#include <cstdint>
#include "FScreenPassTexture.h"

struct FScreenPassRenderTarget : FScreenPassTexture {
    uint8_t LoadAction;
};
