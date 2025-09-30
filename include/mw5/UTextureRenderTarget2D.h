#pragma once
#include <cstdint>

struct FTextureResource;

struct UTextureRenderTarget2D {
    uint64_t          Pad000[19];
    FTextureResource* Resource;
    uint64_t          Pad0A0[8];
    int32_t           SizeX;
    int32_t           SizeY;
};
