#pragma once
#include <cstdint>
#include "FTransform.h"

struct USceneComponent {
	uint64_t   Pad000[56];
	FTransform ComponentToWorld;
};
