#pragma once

struct FRHITexture2D;

struct FRDGTexture {
    uint64_t       Pad000[2];
    FRHITexture2D* TextureRHI;
};
