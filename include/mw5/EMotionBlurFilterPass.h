#pragma once
#include <cstdint>

enum class EMotionBlurFilterPass : uint32_t {
    Separable0,
    Separable1,
    Unified,
    MAX
};
