#pragma once
#include "FIntPoint.h"
#include "FIntRect.h"

struct FScreenPassTextureViewport
{
    FIntPoint Extent;
    FIntRect  Rect;
};

static_assert(sizeof(FScreenPassTextureViewport) == 0x18);
