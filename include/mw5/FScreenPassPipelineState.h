#pragma once
#include "TShaderRefBase.h"

struct FScreenPassPipelineState {
    // Dodgy, not pointer arrays but will fix later
    TShaderRefBase VertexShader;
    TShaderRefBase PixelShader;
    void*          BlendState;
    void*          DepthStencilState;
    void*          VertexDeclaration;
};

static_assert(sizeof(FScreenPassPipelineState) == 0x38);
