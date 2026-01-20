#include <PluginExtension.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

class HeadAim final : public PluginExtension {
    enum class HeadAimMode : uint8_t {
        Disabled, ArmsOnly, TorsoAndArms
    };

    API::UObject* Pawn                   = nullptr;
    HeadAimMode*  HeadAimMode            = nullptr;
    vec2*         ArmTwist               = nullptr;
    vec3*         TargetViewRotator      = nullptr; // Pitch, yaw, roll
    vec3*         LastViewRotator        = nullptr;
    bool*         ArmlockEnabled         = nullptr;
    bool*         EnableArmLock          = nullptr;
    float*        ZoomLevel              = nullptr;
    float*        HeadAimPitchOffset     = nullptr;
    float*        HeadAimYawOffset       = nullptr;
    vec3*         HeadCrosshairRotator   = nullptr; // Pitch, yaw, roll
    vec2*         TorsoRotation          = nullptr;
    API::UObject* VRHeadCrosshairRotator = nullptr;
    vec3*         MechRelativeRotation   = nullptr;
    vec2*         HeadTarget             = nullptr;
    vec2*         ArmsTarget             = nullptr;
    bool*         HeadAimLocked          = nullptr;
    void**        CurrentTarget          = nullptr;
    void**        PreviousFriendlyTarget = nullptr;
    void**        PreviousHostileTarget  = nullptr;
    void**        PendingLockTarget      = nullptr;
    void**        LockedOnTargetOverride = nullptr;

    bool  InMech         = false;
    bool  HooksInstalled = false;
    float Delta          = 0;
    vec2  RotationSpeed  = vec2(0, 0);

#define ARM_SPRING_CONSTANT 175.0f
#define ARM_DAMPING_CONSTANT 25.0f

    vec2 CurrentVelocity{};
    vec2 CurrentRotation{};
    vec2 SmoothedHeadRotation{};

    struct TwistBounds {
        // Yaw, Pitch
        vec2 TwistRate;
        bool IsYawUnbound;
        bool IsPitchUnbound;
        vec2 BoundsLow;
        vec2 BoundsHigh;
    };

    struct TorsoStats {
        TwistBounds Torso;
        TwistBounds Arm;
    };

    TorsoStats* TorsoStats = nullptr;

public:
    inline static HeadAim* Instance;

    HeadAim() {
        Instance                  = this;
        PluginExtension::Instance = this;
        Name                      = "HeadAim";
        Version                   = "2.1.0";
        VersionInt                = 210;
        VersionCheckFnName        = L"OnFetchHeadAimPluginData";
        VersionPropertyName       = L"HeadAimVersion";
    }

    virtual void on_pre_engine_tick(API::UGameEngine* engine, const float delta) override {
        if (const auto activePawn = API::get()->get_local_pawn(0); activePawn != Pawn) {
            InMech = OnNewPawn(activePawn);
        }

        this->Delta = delta;

        if (InMech && *HeadAimMode == HeadAimMode::ArmsOnly) {
            // LastRelativeViewRotator is used by the targeting logic, setting this here doesn't seem to affect anything else but targeting
            *LastViewRotator = vec3(HeadTarget->y + TorsoRotation->y, HeadTarget->x + TorsoRotation->x, 0.0f);

            // Force disable arm lock, it's being handled manually
            *ArmlockEnabled = false;
            *EnableArmLock  = true;
        }
    }

    static void* OnTorsoTwist(API::UObject*, FFrame*, void* const) {
        if (*Instance->HeadAimMode == HeadAimMode::Disabled)
            return nullptr;
        Instance->ProcessHeadAim();
        return nullptr;
    }

    static void* OnSetTarget(API::UObject*, FFrame* frame, void* const) {
        Instance->LogInfo("OnSetTarget called");
        *Instance->CurrentTarget          = *frame->GetParams<void**>();
        *Instance->PreviousFriendlyTarget = nullptr;
        *Instance->PreviousHostileTarget  = nullptr;
        *Instance->PendingLockTarget      = nullptr;
        *Instance->LockedOnTargetOverride = nullptr;
        return nullptr;
    }

private:
    bool OnNewPawn(API::UObject* activePawn) {
        Pawn = activePawn;
        if (!activePawn)
            return false;

        API::UObject* mechViewC,* mechMeshC,* cockpitC,* torsoTwistC,* mechCockpit,* userSettings,* vrHUDManager,* mechRootC,* targetTrackingC,* lockOnC,* vrHeadAimHandler;

        const auto playerController = API::get()->get_player_controller(0);

        bool success = TryGetProperty(Pawn, L"MechViewComponent", mechViewC) && TryGetProperty(Pawn, L"MechMeshComponent", mechMeshC);

        if (!success) {
            LogInfo("Failed to get Mech references - probably not in a Mech");
            return false;
        }

        success = TryGetProperty(mechMeshC, L"FirstPersonCockpitComponent", cockpitC) &&
                  TryGetProperty(cockpitC, L"ChildActor", mechCockpit) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadAimMode", HeadAimMode) &&
                  TryGetProperty(Pawn, L"TorsoTwistComponent", torsoTwistC) &&
                  TryGetPropertyStruct(torsoTwistC, L"bArmlockEnabled", ArmlockEnabled) &&
                  TryGetPropertyStruct(torsoTwistC, L"ArmTwist", ArmTwist) &&
                  TryGetPropertyStruct(mechViewC, L"TargetRelativeViewRotator", TargetViewRotator) &&
                  TryGetPropertyStruct(mechViewC, L"LastRelativeViewRotator", LastViewRotator) &&
                  TryGetPropertyStruct(torsoTwistC, L"TorsoStats", TorsoStats) &&
                  TryGetProperty(playerController, L"MWGameUserSettings", userSettings) &&
                  TryGetPropertyStruct(userSettings, L"EnableArmLock", EnableArmLock) &&
                  TryGetProperty(mechCockpit, L"VR_HUDManager", vrHUDManager) &&
                  TryGetPropertyStruct(vrHUDManager, L"ZoomLevel", ZoomLevel) &&
                  TryGetProperty(mechCockpit, L"VR_HeadCrosshairRotator", VRHeadCrosshairRotator) &&
                  TryGetPropertyStruct(VRHeadCrosshairRotator, L"RelativeRotation", HeadCrosshairRotator) &&
                  TryGetPropertyStruct(torsoTwistC, L"TorsoTwist", TorsoRotation) &&
                  TryGetProperty(Pawn, L"RootComponent", mechRootC) &&
                  TryGetPropertyStruct(mechRootC, L"RelativeRotation", MechRelativeRotation) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadAimPitchOffset", HeadAimPitchOffset) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadAimYawOffset", HeadAimYawOffset) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadTarget", HeadTarget) &&
                  TryGetPropertyStruct(mechCockpit, L"ArmsTarget", ArmsTarget) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadAimLocked", HeadAimLocked) &&
                  TryGetProperty(Pawn, L"TargetTracking", targetTrackingC) &&
                  TryGetPropertyStruct(targetTrackingC, L"CurrentTarget", CurrentTarget) &&
                  TryGetPropertyStruct(targetTrackingC, L"PreviousFriendlyTarget", PreviousFriendlyTarget) &&
                  TryGetPropertyStruct(targetTrackingC, L"PreviousHostileTarget", PreviousHostileTarget) &&
                  TryGetProperty(Pawn, L"LockOnComponent", lockOnC) &&
                  TryGetPropertyStruct(lockOnC, L"PendingLockTarget", PendingLockTarget) &&
                  TryGetPropertyStruct(lockOnC, L"LockedOnTargetOverride", LockedOnTargetOverride);

        if (!success) {
            LogInfo("Failed to get cockpit references, retrying next frame...");
            Pawn = nullptr;
            return false;
        }

        if (!HooksInstalled) {
            if (!AddEventHook(mechCockpit->get_class(), L"OnTorsoTwist", &OnTorsoTwist))
                return false;

            if (!AddEventHook(mechCockpit->get_class(), L"OnSetTarget", &OnSetTarget))
                return false;

            HooksInstalled = true;
        }

        LogInfo("Mech references found and hooks enabled");
        return true;
    }

    static vec3 EulerAnglesFromQuat(const quat& q) {
        const auto rot   = mat4{q};
        float      pitch = 0.0f;
        float      yaw   = 0.0f;
        float      roll  = 0.0f;
        extractEulerAngleYXZ(rot, yaw, pitch, roll);
        return {pitch, -yaw, -roll};
    }

    vec3 GetHMDRotation() const {
        UEVR_Vector3f    pose;
        UEVR_Quaternionf rot, offset;
        const auto       hmdIndex = API::get()->param()->vr->get_hmd_index();

        API::get()->param()->vr->get_pose(hmdIndex, &pose, &rot);
        API::get()->param()->vr->get_rotation_offset(&offset);

        const quat quatRot(rot.w, rot.x, rot.y, rot.z);
        const quat quatRotOffset(offset.w, offset.x, offset.y, offset.z);
        const quat combinedRot = normalize(quatRotOffset * quatRot);
        vec3       euler       = degrees(EulerAnglesFromQuat(combinedRot));
        euler.x                += *HeadAimPitchOffset;
        euler.y                += *HeadAimYawOffset;

        return euler;
    }

    void ProcessHeadAim() {
        const vec3 hmdRotation = GetHMDRotation();

        UpdateHeadAimTarget({hmdRotation.y, hmdRotation.x});

        vec2 targetRotation;
        if (*HeadAimLocked) {
            targetRotation = vec2(0, 0);
            *ArmsTarget    = vec2(0, 0);
        } else {
            targetRotation = *ArmsTarget;
        }

        targetRotation /= *ZoomLevel;

        const vec2 rotationDiff = targetRotation - CurrentRotation;
        const vec2 rotationRate = TorsoStats->Arm.TwistRate;

        const vec2 force = ARM_SPRING_CONSTANT * rotationDiff - ARM_DAMPING_CONSTANT * CurrentVelocity;
        CurrentVelocity  += force * Delta;
        CurrentVelocity  = clamp(CurrentVelocity, -rotationRate, rotationRate);
        CurrentRotation  += CurrentVelocity * Delta;

        CurrentRotation = {
            clamp(CurrentRotation.x, TorsoStats->Arm.BoundsLow.x, TorsoStats->Arm.BoundsHigh.x),
            clamp(CurrentRotation.y, TorsoStats->Arm.BoundsLow.y, TorsoStats->Arm.BoundsHigh.y)
        };

        *ArmTwist = CurrentRotation + *TorsoRotation;
    }

    void UpdateHeadAimTarget(const vec2& targetHeadRotation) {
        constexpr float maxDistance = 2.0f;
        constexpr float minInterp   = 0.05f;
        constexpr float maxInterp   = 1.0f;

        const float distance        = length(targetHeadRotation - SmoothedHeadRotation);
        const float smoothingFactor = minInterp + min((distance / maxDistance), 1.0f) * (maxInterp - minInterp);

        SmoothedHeadRotation = lerp(SmoothedHeadRotation, targetHeadRotation, smoothingFactor);
        *HeadTarget          = SmoothedHeadRotation;
        *ArmsTarget          = {
            clamp(HeadTarget->x, TorsoStats->Arm.BoundsLow.x, TorsoStats->Arm.BoundsHigh.x),
            clamp(HeadTarget->y, TorsoStats->Arm.BoundsLow.y, TorsoStats->Arm.BoundsHigh.y)
        };
    }
};

// ReSharper disable once CppInconsistentNaming
std::unique_ptr<HeadAim> g_plugin{new HeadAim()}; // NOLINT(misc-use-internal-linkage)
