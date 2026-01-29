#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/compatibility.hpp"
#include <PluginExtension.h>
#include <windows.h>
#include <chrono>
#include "Offsets.h"
#include "MW5.h"

class Cameras final : public PluginExtension {
public:
    inline static Cameras* Instance = nullptr;

    Cameras() {
        Instance                  = this;
        PluginExtension::Instance = this;
        Name                      = "Cameras";
        Version                   = "2.1.0";
        VersionInt                = 210;
        VersionCheckFnName        = L"OnFetchCamerasPluginData";
        VersionPropertyName       = L"CamerasVersion";
    }

    virtual ~Cameras() override {
        Instance = nullptr;
        SetCurrentInputTypeHook.Uninstall();
        AddGTAOSpatialFilterHook.Uninstall();
        FSSAOShaderParameters_OperatorEqualsHook.Uninstall();
    }

    enum class EKeyInputType : uint8_t {
        KeyboardMouse = 0, Gamepad = 1, Joystick = 2,
    };

    inline static auto                CachedCameraOffset    = float3(-1.0f, -1.0f, -1.0f);
    inline static auto                CachedCameraPosition  = float3(-1.0f, -1.0f, -1.0f);
    inline static std::optional<bool> CachedCursorEnabled   = std::nullopt;
    inline static bool                ShowCursor            = true;
    inline static bool                VREnabled             = false;
    inline static std::optional<bool> CachedDecoupledPitch  = std::nullopt;
    inline static bool                CachedIsStarmap       = false;
    inline static auto                ActiveInputType       = EKeyInputType::KeyboardMouse;
    inline static bool                EnableVRControllerFix = true;

    using FnA = EKeyInputType(__fastcall *)(void* self, EKeyInputType inputType);
    static inline FunctionHook<FnA> SetCurrentInputTypeHook{"SetCurrentInputType"};

    using FnB = FScreenPassTexture (*)(FRDGBuilder&            graphBuilder,
                                       const FViewInfo&        view,
                                       const void*             commonParameters,
                                       FScreenPassTexture      input,
                                       FScreenPassTexture      inputDepth,
                                       FScreenPassRenderTarget suggestedOutput);
    static inline FunctionHook<FnB> AddGTAOSpatialFilterHook{"AddGTAOSpatialFilter"};

    using FnC = void(__fastcall *)(FSSAOShaderParameters* self, const FSSAOShaderParameters& other);
    static inline FunctionHook<FnC> FSSAOShaderParameters_OperatorEqualsHook{"FSSAOShaderParameters::operator="};

    virtual void OnInitialize() override {
        const auto vrGlobal = API::get()->find_uobject<API::UClass>(L"BlueprintGeneratedClass /Game/MechWarriorVR/VR_Global.VR_Global_C");
        if (!vrGlobal) {
            LogError("Failed to find VR Global class");
            return;
        }

        if (!AddEventHook(vrGlobal, L"SetCameraOffset", &OnSetCameraOffset))
            return;

        if (!AddEventHook(vrGlobal, L"SetDecoupledPitch", &OnSetDecoupledPitch)) {
            RemoveAllEventHooks(true);
            return;
        }

        if (!AddEventHook(vrGlobal, L"SetMouseEnabled", &OnSetMouseEnabled)) {
            RemoveAllEventHooks(true);
            return;
        }

        if (!AddEventHook(vrGlobal, L"SetIsStarmap", &OnSetIsStarmap)) {
            RemoveAllEventHooks(true);
            return;
        }

        if (!AddEventHook(vrGlobal, L"SendCameraPosition", &OnSendCameraPosition)) {
            RemoveAllEventHooks(true);
            return;
        }

        if (!AddEventHook(vrGlobal, L"SetEnableVRControllerFix", &OnSetEnableVRControllerFix)) {
            RemoveAllEventHooks(true);
            return;
        }

        const auto vrMechCockpit = API::get()->find_uobject<API::UClass>(L"BlueprintGeneratedClass /Game/MechWarriorVR/VR_Mech_Cockpit.VR_Mech_Cockpit_C");
        if (!vrGlobal) {
            LogError("Failed to find VR Mech Cockpit class");
            RemoveAllEventHooks(true);
            return;
        }

        if (!AddEventHook(vrMechCockpit, L"SetDirectionalLightParams", &OnSetDirectionalLightParams)) {
            RemoveAllEventHooks(true);
            return;
        }

        Offsets::FindAll();
        SetCurrentInputTypeHook.DetourOffset(Offsets::MWInputListener_SetCurrentInputType_Offset, &MWInputListener_SetCurrentInputType);
        AddGTAOSpatialFilterHook.DetourOffset(Offsets::AddGTAOSpatialFilter_Offset, &AddGTAOSpatialFilter);
        FSSAOShaderParameters_OperatorEqualsHook.DetourOffset(Offsets::FSSAOShaderParameters_OperatorEquals_Offset, &FSSAOShaderParameters_OperatorEquals);
    }

    virtual void on_pre_engine_tick(API::UGameEngine* engine, float delta) override {
        static bool initialised = false;
        if (!initialised) {
            initialised = true;
            SetDefaults();
        }
    }

    static void SetDefaults() {
        const auto vr = API::get()->param()->vr;
        vr->set_mod_value("UI_Distance", "1.35");
        vr->set_mod_value("UI_Size", "1.2");
        vr->set_mod_value("UI_Framework_Distance", "1.35");
        vr->set_mod_value("UI_Framework_Size", "1.2");
        vr->set_mod_value("VR_CameraForwardOffset", "0.000");
        vr->set_mod_value("VR_CameraRightOffset", "0.000");
        vr->set_mod_value("VR_CameraUpOffset", "0.000");
        vr->set_decoupled_pitch_enabled(false);
        vr->set_mod_value("VR_DecoupledPitchUIAdjust", "false");
        vr->set_mod_value("FrameworkConfig_AlwaysShowCursor", "true");
    }

    static void FSSAOShaderParameters_OperatorEquals(FSSAOShaderParameters* self, const FSSAOShaderParameters& other) {
        FSSAOShaderParameters_OperatorEqualsHook.OriginalFn(self, other);

        if ((uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandleW(nullptr) != Offsets::AddGTAOHorizonSearchIntegratePass_FSSAOShaderParameters_OperatorEquals_Callsite_Offset)
            return;

        const auto params = (FGTAOHorizonSearchAndIntegrateCSParameters*)((uintptr_t)self - 0x30);
        if (params->SSAOParameters.AOViewport.ViewportMin.X == 0)
            return;

        const auto hzbScaleX = params->HZBParameters.HZBRemapping.Scale.X;
        // Fixes an Unreal GTAO bug where the right eye is sampling the wrong location on the HZB texture
        params->HZBParameters.HZBRemapping.Bias.X = hzbScaleX * -0.5f;
    }

    static FScreenPassTexture AddGTAOSpatialFilter(FRDGBuilder&            graphBuilder,
                                                   const FViewInfo&        view,
                                                   const void*             commonParameters,
                                                   FScreenPassTexture      input,
                                                   FScreenPassTexture      inputDepth,
                                                   FScreenPassRenderTarget suggestedOutput) {
        // Fixes an Unreal GTAO bug where the input and inputDepth parameters were swapped and caused the entire screen to darken
        return AddGTAOSpatialFilterHook.OriginalFn(graphBuilder, view, commonParameters, inputDepth, input, suggestedOutput);
    }

    static void* OnSetCameraOffset(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const float3* offset = frame->GetParams<float3>();
        if (!offset || all(equal(CachedCameraOffset, *offset)))
            return nullptr;

        const auto vr = API::get()->param()->vr;
        vr->set_mod_value("VR_CameraForwardOffset", std::to_string(offset->x).c_str());
        vr->set_mod_value("VR_CameraRightOffset", std::to_string(offset->y).c_str());
        vr->set_mod_value("VR_CameraUpOffset", std::to_string(offset->z).c_str());

        CachedCameraOffset = *offset;

        Instance->LogInfo("Camera offsets set to Forward: %.2f, Right: %.2f, Up: %.2f", offset->x, offset->y, offset->z);
        return nullptr;
    }

    static void* OnSetDecoupledPitch(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const bool* decoupled = frame->GetParams<bool>();
        if (!decoupled || CachedDecoupledPitch == *decoupled)
            return nullptr;

        const auto vr = API::get()->param()->vr;
        vr->set_decoupled_pitch_enabled(*decoupled);
        vr->set_mod_value("VR_DecoupledPitchUIAdjust", "false");

        CachedDecoupledPitch = *decoupled;

        Instance->LogInfo("Decoupled pitch set to: %s", *decoupled ? "true" : "false");
        return nullptr;
    }

    static EKeyInputType MWInputListener_SetCurrentInputType(void* self, EKeyInputType inputType) {
        if (!VREnabled || !EnableVRControllerFix)
            return SetCurrentInputTypeHook.OriginalFn(self, inputType);

        static POINT previousMousePosition = {};

        static std::chrono::steady_clock::time_point lastMouseMove = std::chrono::steady_clock::now();

        POINT currentMousePosition;
        GetCursorPos(&currentMousePosition);

        const auto now = std::chrono::steady_clock::now();

        if (currentMousePosition.x != previousMousePosition.x || currentMousePosition.y != previousMousePosition.y) {
            lastMouseMove         = now;
            previousMousePosition = currentMousePosition;
        }

        const bool mouseMovedRecently = now - lastMouseMove <= std::chrono::seconds(3);

        if (mouseMovedRecently)
            ActiveInputType = EKeyInputType::KeyboardMouse;
        else if (API::get()->param()->vr->is_using_controllers())
            ActiveInputType = EKeyInputType::Gamepad;

        UpdateCursorState();

        return SetCurrentInputTypeHook.OriginalFn(self, ActiveInputType);
    }

    struct SetMouseEnabledData {
        bool Enabled;
        bool VREnabled;
    };

    static void* OnSetMouseEnabled(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const SetMouseEnabledData* mouseEnabled = frame->GetParams<SetMouseEnabledData>();
        if (!mouseEnabled)
            return nullptr;

        ShowCursor = mouseEnabled->Enabled;
        VREnabled  = mouseEnabled->VREnabled;

        UpdateCursorState();
        return nullptr;
    }

    static void UpdateCursorState() {
        const bool wantCursor = (ActiveInputType == EKeyInputType::KeyboardMouse) && ShowCursor && VREnabled;

        if (CachedCursorEnabled && *CachedCursorEnabled == wantCursor)
            return;

        API::get()->param()->vr->set_mod_value("FrameworkConfig_AlwaysShowCursor", wantCursor ? "true" : "false");

        CachedCursorEnabled = wantCursor;
    }

    static void* OnSetIsStarmap(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const bool* isStarmap = frame->GetParams<bool>();
        if (!isStarmap || CachedIsStarmap == *isStarmap)
            return nullptr;

        if (!*isStarmap) {
            const auto vr = API::get()->param()->vr;
            vr->set_mod_value("UI_Distance", "1.35");
            vr->set_mod_value("UI_Size", "1.2");
            vr->set_mod_value("UI_Framework_Distance", "1.35");
            vr->set_mod_value("UI_Framework_Size", "1.2");
        }

        CachedIsStarmap = *isStarmap;

        Instance->LogInfo("IsStarmap set to: %s", *isStarmap ? "true" : "false");
        return nullptr;
    }

    static void* OnSendCameraPosition(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const float3* cameraPosition = frame->GetParams<float3>();
        if (!cameraPosition || all(equal(CachedCameraPosition, *cameraPosition)))
            return nullptr;


        const float cameraPositionZ = cameraPosition->z * 0.01f;
        const auto  offset          = float3(-cameraPosition->z * 0.5f, 0.0f, 0.0f); // Zoom out a bit
        const float distance        = cameraPositionZ - offset.x * 0.01f;
        const float size            = cameraPositionZ * 1.125f;
        const auto  distanceStr     = std::to_string(distance);
        const auto  sizeStr         = std::to_string(size);

        const auto vr = API::get()->param()->vr;
        vr->set_mod_value("UI_Distance", distanceStr.c_str());
        vr->set_mod_value("UI_Size", sizeStr.c_str());
        vr->set_mod_value("UI_Framework_Distance", distanceStr.c_str());
        vr->set_mod_value("UI_Framework_Size", sizeStr.c_str());
        vr->set_mod_value("VR_CameraForwardOffset", std::to_string(offset.x).c_str());
        vr->set_mod_value("VR_CameraRightOffset", std::to_string(offset.y).c_str());
        vr->set_mod_value("VR_CameraUpOffset", std::to_string(offset.z).c_str());

        CachedCameraPosition = *cameraPosition;
        CachedCameraOffset   = offset;

        return nullptr;
    }

    static void* OnSetEnableVRControllerFix(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const bool* enableVrControllerFix = frame->GetParams<bool>();
        if (!enableVrControllerFix)
            return nullptr;

        EnableVRControllerFix = *enableVrControllerFix;

        Instance->LogInfo("EnableVRControllerFix set to: %s", EnableVRControllerFix ? "true" : "false");
        return nullptr;
    }

    struct SetDirectionalLightParams {
        API::UObject* DirectionalLight;
        int32_t       FarShadowCascadeCount;
        float         CockpitWorldScale;
    };

    static void* OnSetDirectionalLightParams(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        const auto* params = frame->GetParams<SetDirectionalLightParams>();
        if (!params)
            return nullptr;

        int32_t* farCascadeCount = nullptr;
        if (Instance->TryGetPropertyStruct(params->DirectionalLight, L"FarShadowCascadeCount", farCascadeCount)) {
            *farCascadeCount = params->FarShadowCascadeCount;
            Instance->LogInfo("FarShadowCascadeCount set to: %d", params->FarShadowCascadeCount);
        }

        float* shadowSharpen = nullptr;
        if (Instance->TryGetPropertyStruct(params->DirectionalLight, L"ShadowSharpen", shadowSharpen)) {
            *shadowSharpen = 0.1f;
            Instance->LogInfo("ShadowSharpen set to: 0.1f");
        }

        float* dynamicShadowDistanceMovableLight = nullptr;
        if (Instance->TryGetPropertyStruct(params->DirectionalLight, L"DynamicShadowDistanceMovableLight", dynamicShadowDistanceMovableLight)) {
            *dynamicShadowDistanceMovableLight = params->CockpitWorldScale * 3.7f;
            Instance->LogInfo("Cockpit shadow distance set to: %f", *dynamicShadowDistanceMovableLight);
        }

        return nullptr;
    }
};

// ReSharper disable once CppInconsistentNaming
std::unique_ptr<Cameras> g_plugin{new Cameras()}; // NOLINT(misc-use-internal-linkage)
