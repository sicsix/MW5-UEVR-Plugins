#pragma once
#include <cstdint>
#include "ERDGPassFlags.h"
#include "FRDGEventName.h"
#include "FRDGParameterStruct.h"

struct FRDGPass {
    void**                    VTable;
    const FRDGEventName       Name;
    const FRDGParameterStruct ParameterStruct;
    const ERDGPassFlags       Flags;
    uint64_t                  Pad000[25];
};

static_assert(sizeof(FRDGPass) == 0xF0);
