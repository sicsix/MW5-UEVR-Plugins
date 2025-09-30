#pragma once
#include <cstdint>
#include "FScreenPassRenderTarget.h"
#include "FScreenPassTexture.h"

struct FMotionBlurInputs {
    FScreenPassRenderTarget OverrideOutput;
    FScreenPassTexture      SceneColor;
    FScreenPassTexture      SceneDepth;
    FScreenPassTexture      SceneVelocity;
    uint32_t                Quality;
    uint32_t                Filter;
};

static_assert(sizeof(FMotionBlurInputs) == 0x70);
