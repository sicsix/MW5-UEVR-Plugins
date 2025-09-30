#pragma once
#include <cstdint>

struct FResolveRect {
    int32_t X1;
    int32_t Y1;
    int32_t X2;
    int32_t Y2;
};

static_assert(sizeof(FResolveRect) == 0x10);
