#pragma once
#include <cstdint>
#include "FIntPoint.h"

struct FIntRect {
    FIntPoint Min;
    FIntPoint Max;

    int32_t Width() const {
        return Max.X - Min.X;
    }

    int32_t Height() const {
        return Max.Y - Min.Y;
    }

    bool IsEmpty() const {
        return Width() == 0 && Height() == 0;
    }
};

static_assert(sizeof(FIntRect) == 0x10);
