#pragma once
#include <cstdint>
#include "FPrimitiveComponentId.h"

struct FWidget3DSceneProxy;

struct UPrimitiveComponent {
	uint64_t              Pad000[79];
	uint32_t              Pad160[1];
	FPrimitiveComponentId ComponentId;
	uint64_t              Pad001[55];
	FWidget3DSceneProxy*  SceneProxy;
};
