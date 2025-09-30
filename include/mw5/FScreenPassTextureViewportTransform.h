#pragma once
#include "FVector2D.h"

struct FScreenPassTextureViewportTransform {
    FVector2D Scale;
    FVector2D Bias;
};

static_assert(sizeof(FScreenPassTextureViewportTransform) == 0x10);
