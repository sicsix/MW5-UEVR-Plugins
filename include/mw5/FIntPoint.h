#pragma once
#include <cstdint>

struct FIntPoint {
    int32_t X;
    int32_t Y;
};

static_assert(sizeof(FIntPoint) == 0x8);
