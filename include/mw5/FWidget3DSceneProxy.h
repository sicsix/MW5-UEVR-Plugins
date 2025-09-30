#pragma once
#include <cstdint>
#include "RE.h"
#include "UTextureRenderTarget2D.h"
#include <glm/glm.hpp>
using namespace glm;

struct FWidget3DSceneProxy {
    uint64_t                Pad000[18];
    mat4                    LocalToWorld; // + 16 offset from UE4
    uint64_t                Pad001[8];
    uint32_t                Pad002[1];
    FPrimitiveComponentId   PrimitiveComponentId;
    uint64_t                Pad003[9]; // FPrimitiveSceneProxy ends here
    vec3                    Origin;
    float                   ArcAngle;
    vec2                    Pivot;
    void*                   Renderer;
    UTextureRenderTarget2D* RenderTarget;
    void*                   MaterialInstance;
    uint64_t                MaterialRelevance;
    EWidgetBlendMode        BlendMode;
    uint8_t                 GeometryMode;
    void*                   BodySetup;

    using Fn11 = void(__fastcall *)(FWidget3DSceneProxy*             self,
                                    const TArray<const FSceneView*>& views,
                                    const FSceneViewFamily&          viewFamily,
                                    uint32_t                         visibilityMap,
                                    void*                            collector);
    static inline VTableHook<Fn11> GetDynamicMeshElements{"FWidget3DSceneProxy::GetDynamicMeshElements", 11};

    using Fn26 = bool(__fastcall *)(FWidget3DSceneProxy* self);
    static inline VTableHook<Fn26> CanBeOccluded{"FWidget3DSceneProxy::CanBeOccluded", 26};
};
