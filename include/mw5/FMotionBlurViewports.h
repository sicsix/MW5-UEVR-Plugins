#pragma once
#include "mw5/FScreenPassTextureViewport.h"
#include "mw5/FScreenPassTextureViewportParameters.h"
#include "mw5/FScreenPassTextureViewportTransform.h"

struct alignas(16) FMotionBlurViewports {
    FScreenPassTextureViewport           Color;
    FScreenPassTextureViewport           Velocity;
    FScreenPassTextureViewport           VelocityTile;
    FScreenPassTextureViewportParameters ColorParameters;
    FScreenPassTextureViewportParameters VelocityParameters;
    FScreenPassTextureViewportParameters VelocityTileParameters;
    FScreenPassTextureViewportTransform  ColorToVelocityTransform;
    FScreenPassTextureViewportTransform  ColorToVelocityTileTransform;
};

static_assert(sizeof(FMotionBlurViewports) == 0x1C0);
