#pragma once

struct FMotionBlurParameters
{
    float AspectRatio;
    float VelocityScale;
    float VelocityScaleForTiles;
    float VelocityMax;
};

static_assert(sizeof(FMotionBlurParameters) == 0x10);