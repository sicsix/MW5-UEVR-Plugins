#pragma once
#include <cstdint>

struct FRDGTexture;

struct FDepthStencilBinding {
	FRDGTexture* Texture;
	uint8_t      DepthLoadAction;
	uint8_t      StencilLoadAction;
	uint32_t     DepthStencilAccess;
};

static_assert(sizeof(FDepthStencilBinding) == 0x10);
