#pragma once
#include "FMotionBlurParameters.h"
#include "FScreenPassTextureViewportParameters.h"
#include "FScreenPassTextureViewportTransform.h"

struct FRDGTexture;
struct FRHISamplerState;

struct alignas(16) FMotionBlurFilterParameters {
    FMotionBlurParameters                MotionBlur;
    FScreenPassTextureViewportParameters Color;
    FScreenPassTextureViewportParameters Velocity;
    FScreenPassTextureViewportParameters VelocityTile;
    FScreenPassTextureViewportTransform  ColorToVelocity;
    FScreenPassTextureViewportTransform  ColorToVelocityTile;
    FRDGTexture*                         ColorTexture;
    FRDGTexture*                         VelocityFlatTexture;
    FRDGTexture*                         VelocityTileTexture;
    FRHISamplerState*                    ColorSampler;
    FRHISamplerState*                    VelocitySampler;
    FRHISamplerState*                    VelocityTileSampler;
    FRHISamplerState*                    VelocityFlatSampler;
};

static_assert(sizeof(FMotionBlurFilterParameters) == 0x1C0);
