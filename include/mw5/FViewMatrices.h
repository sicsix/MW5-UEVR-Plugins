#pragma once
#include <glm/glm.hpp>
using namespace glm;

struct FViewMatrices {
	mat4  ProjectionMatrix;
	mat4  ProjectionNoAAMatrix;
	mat4  InvProjectionMatrix;
	mat4  ViewMatrix;
	mat4  InvViewMatrix;
	mat4  ViewProjectionMatrix;
	mat4  InvViewProjectionMatrix;
	mat4  HMDViewMatrixNoRoll;
	mat4  TranslatedViewMatrix;
	mat4  InvTranslatedViewMatrix;
	mat4  OverriddenTranslatedViewMatrix;
	mat4  OverriddenInvTranslatedViewMatrix;
	mat4  TranslatedViewProjectionMatrix;
	mat4  InvTranslatedViewProjectionMatrix;
	vec3  PreViewTranslation;
	vec3  ViewOrigin;
	vec2  ProjectionScale;
	vec2  TemporalAAProjectionJitter;
	float ScreenScale;
};
