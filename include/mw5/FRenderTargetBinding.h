#pragma once
#include <cstdint>

struct alignas(8) FRenderTargetBinding {
	FRDGTexture* Texture;
	FRDGTexture* ResolveTexture;
	uint8_t      LoadAction;
	uint8_t      MipIndex;
	uint16_t     ArraySlice;
};

static_assert(sizeof(FRenderTargetBinding) == 0x18);
