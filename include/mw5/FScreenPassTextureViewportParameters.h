#pragma once
#include "FIntPoint.h"
#include "FVector2D.h"

struct FScreenPassTextureViewportParameters {
    FVector2D Extent;
    FVector2D ExtentInverse;
    FVector2D ScreenPosToViewportScale;
    FVector2D ScreenPosToViewportBias;
    FIntPoint ViewportMin;
    FIntPoint ViewportMax;
    FVector2D ViewportSize;
    FVector2D ViewportSizeInverse;
    FVector2D UVViewportMin;
    FVector2D UVViewportMax;
    FVector2D UVViewportSize;
    FVector2D UVViewportSizeInverse;
    FVector2D UVViewportBilinearMin;
    FVector2D UVViewportBilinearMax;
};

static_assert(sizeof(FScreenPassTextureViewportParameters) == 0x70);
