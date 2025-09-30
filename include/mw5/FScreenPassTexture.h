#pragma once
#include "FIntRect.h"

struct FRDGTexture;

struct FScreenPassTexture {
    FRDGTexture* Texture;
    FIntRect     ViewRect;

    bool IsValid() const {
        return Texture != nullptr && !ViewRect.IsEmpty();
    }
};

static_assert(sizeof(FScreenPassTexture) == 0x18);
