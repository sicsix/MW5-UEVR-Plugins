#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
using namespace glm;

struct alignas(16) FTransform {
    quat Rotation;
    vec3 Translation;
    vec3 Scale3D;
};

static_assert(sizeof(FTransform) == 0x30);
