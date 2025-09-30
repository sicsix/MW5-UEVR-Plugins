#pragma once
#include <cstdint>

struct FRHITexture2D;

struct FTextureResource {
	uint64_t       Pad000[18];
	FRHITexture2D* Texture2DRHI;
};
