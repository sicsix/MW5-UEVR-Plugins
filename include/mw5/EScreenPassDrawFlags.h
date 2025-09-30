#pragma once
#include <cstdint>

enum class EScreenPassDrawFlags : uint8_t {
	None, FlipYAxis = 0x1, AllowHMDHiddenAreaMask = 0x2
};
