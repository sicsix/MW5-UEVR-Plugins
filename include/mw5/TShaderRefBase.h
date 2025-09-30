#pragma once

struct TShaderRefBase {
    void* ShaderContent;
    void* ShaderMap;
};

static_assert(sizeof(TShaderRefBase) == 0x10);
