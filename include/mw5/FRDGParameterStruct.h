#pragma once
#include <cstdint>

struct FRHIUniformBufferLayout;

struct FRDGParameterStruct {
    const uint8_t*                 Contents;
    const FRHIUniformBufferLayout* Layout;
};

static_assert(sizeof(FRDGParameterStruct) == 0x10);
