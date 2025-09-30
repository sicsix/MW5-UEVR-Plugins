#pragma once
#include <RE.h>

namespace PostProcessMotionBlur {
    using FnA = FScreenPassTexture*(__fastcall *)(FScreenPassTexture* output, FRDGBuilder* graphBuilder, const FViewInfo* view, const FMotionBlurInputs* inputs);
    static inline FunctionHook<FnA> AddMotionBlurPass{"AddMotionBlurPass"};

    using FnB = void(__fastcall *)(FRDGBuilder*                graphBuilder,
                                   const FViewInfo&            view,
                                   const FMotionBlurViewports& viewports,
                                   FRDGTexture*                colorTexture,
                                   FRDGTexture*                depthTexture,
                                   FRDGTexture*                velocityTexture,
                                   FRDGTexture**               velocityFlatTextureOutput,
                                   FRDGTexture**               velocityTileTextureOutput);
    static inline FunctionHook<FnB> AddMotionBlurVelocityPass{"AddMotionBlurVelocityPass"};

    using FnC = FRDGTexture*(__fastcall *)(FRDGBuilder*                graphBuilder,
                                           const FViewInfo&            view,
                                           const FMotionBlurViewports& viewports,
                                           FRDGTexture*                colorTexture,
                                           FRDGTexture**               velocityFlatTexture,
                                           FRDGTexture**               velocityTileTexture,
                                           EMotionBlurFilterPass       motionBlurFilterPass,
                                           EMotionBlurQuality          motionBlurQuality);
    static inline FunctionHook<FnC> AddMotionBlurFilterPass{"AddMotionBlurFilterPass"};
}

struct FMotionBlurFilterPS {
    struct FParameters {
        FMotionBlurFilterParameters Filter;
        FRenderTargetBindingSlots   RenderTargets;
    };

    static_assert(sizeof(FParameters) == 0x2B0);

    struct alignas(16) AddDrawScreenPassLambda {
        FViewInfo*                 View;
        FScreenPassTextureViewport OutputViewport;
        FScreenPassTextureViewport InputViewport;
        FScreenPassPipelineState   PipelineState;
        TShaderRefBase             PixelShader;
        FParameters*               PixelShaderParameters;
        EScreenPassDrawFlags       DrawFlags;
    };

    struct TRDGLambdaPass : FRDGPass {
        AddDrawScreenPassLambda ExecuteLambda; // 240

        // UE4.26 0x1685DC0 | Steam 0x1C37F40
        // ?ExecuteImpl@?$TRDGLambdaPass@VFParameters@FMotionBlurFilterPS@@V_lambda_312a03fd0120754060f55fa74c89e0ad_@@@@EEAAXAEAVFRHIComputeCommandList@@@Z
        using Fn1 = void(__fastcall *)(TRDGLambdaPass* self, FRHICommandList* rhiCmdList);
        static inline FunctionHook<Fn1> ExecuteImpl{"FMotionBlurFilterPS::TRDGLambdaPass::ExecuteImpl"};
    };
};
