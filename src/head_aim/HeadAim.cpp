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
    vec3*         LastViewRotator        = nullptr;
    bool*         ArmlockEnabled         = nullptr;
    bool*         EnableArmLock          = nullptr;
    float*        ZoomLevel              = nullptr;
    float*        HeadAimPitchOffset     = nullptr;
    vec2*         TorsoRotation          = nullptr;
    vec3*         MechRelativeRotation   = nullptr;
    vec2*         HeadTarget             = nullptr;
    vec2*         ArmsTarget             = nullptr;
    bool*         HeadAimLocked          = nullptr;
    void**        CurrentTarget          = nullptr;
    void**        PreviousFriendlyTarget = nullptr;
    void**        PreviousHostileTarget  = nullptr;
    void**        PendingLockTarget      = nullptr;
    void**        LockedOnTargetOverride = nullptr;
    API::UObject* LockOnComponent        = nullptr;

    bool  InMech                = false;
    bool  HooksInstalled        = false;
    float Delta                 = 0;
    vec2  RotationSpeed         = vec2(0, 0);
    vec2  PreviousTorsoRotation = vec2(0, 0);

#define ARM_SPRING_CONSTANT 200.0f
#define ARM_DAMPING_CONSTANT 25.0f

    vec2 CurrentArmVelocity{};
    vec2 CurrentArmRotation{};
    quat SmoothedHmdQuat  = quat(1, 0, 0, 0);
    bool SmoothedQuatInit = false;

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
        Version                   = "2.2.1";
        VersionInt                = 221;
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

    virtual void OnInitialize() override {
        const auto targetLockHandler = API::get()->find_uobject<API::UClass>(L"BlueprintGeneratedClass /Game/MechWarriorVR/VR_TargetLockHandler.VR_TargetLockHandler_C");
        if (!targetLockHandler) {
            LogError("Failed to find VR Target Lock Handler class");
            return;
        }

        AddEventHook(targetLockHandler, L"OnForceLookingAtTarget", &OnForceLookingAtTarget);
    }

    struct RotationDegrees {
        float Roll;
        float Pitch;
        float Yaw;
    };

    struct OnCalculateHeadAimParams {
        RotationDegrees Cockpit;
        RotationDegrees Torso;
    };

    static void* OnCalculateHeadAim(API::UObject*, FFrame* frame, void* const) {
        if (*Instance->HeadAimMode == HeadAimMode::Disabled)
            return nullptr;
        const auto rotations = *frame->GetParams<OnCalculateHeadAimParams>();
        Instance->ProcessHeadAim();
        Instance->ProcessArmTwist(rotations.Cockpit, rotations.Torso);
        return nullptr;
    }

    static void* OnSetTarget(API::UObject*, FFrame* frame, void* const) {
        *Instance->CurrentTarget          = *frame->GetParams<void**>();
        *Instance->PreviousFriendlyTarget = nullptr;
        *Instance->PreviousHostileTarget  = nullptr;
        *Instance->PendingLockTarget      = nullptr;
        *Instance->LockedOnTargetOverride = nullptr;
        return nullptr;
    }

    struct OnForceLookingAtTargetParams {
        API::UObject* LockOnComponent;
        bool          LookingAtTarget;
    };

    static void* OnForceLookingAtTarget(API::UObject*, FFrame* frame, void* const) {
        const auto params          = *frame->GetParams<OnForceLookingAtTargetParams>();
        bool       lookingAtTarget = params.LookingAtTarget;
        params.LockOnComponent->call_function(L"OnLookingAtTargetChanged", &lookingAtTarget);
        return nullptr;
    }

private:
    bool OnNewPawn(API::UObject* activePawn) {
        Pawn = activePawn;
        if (!activePawn)
            return false;

        API::UObject* mechViewC,* mechMeshC,* cockpitC,* torsoTwistC,* mechCockpit,* userSettings,* vrHUDManager,* mechRootC,* targetTrackingC;

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
                  TryGetPropertyStruct(mechViewC, L"LastRelativeViewRotator", LastViewRotator) &&
                  TryGetPropertyStruct(torsoTwistC, L"TorsoStats", TorsoStats) &&
                  TryGetProperty(playerController, L"MWGameUserSettings", userSettings) &&
                  TryGetPropertyStruct(userSettings, L"EnableArmLock", EnableArmLock) &&
                  TryGetProperty(mechCockpit, L"VR_HUDManager", vrHUDManager) &&
                  TryGetPropertyStruct(vrHUDManager, L"ZoomLevel", ZoomLevel) &&
                  TryGetPropertyStruct(torsoTwistC, L"TorsoTwist", TorsoRotation) &&
                  TryGetProperty(Pawn, L"RootComponent", mechRootC) &&
                  TryGetPropertyStruct(mechRootC, L"RelativeRotation", MechRelativeRotation) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadAimPitchOffset", HeadAimPitchOffset) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadTarget", HeadTarget) &&
                  TryGetPropertyStruct(mechCockpit, L"ArmsTarget", ArmsTarget) &&
                  TryGetPropertyStruct(mechCockpit, L"HeadAimLocked", HeadAimLocked) &&
                  TryGetProperty(Pawn, L"TargetTracking", targetTrackingC) &&
                  TryGetPropertyStruct(targetTrackingC, L"CurrentTarget", CurrentTarget) &&
                  TryGetPropertyStruct(targetTrackingC, L"PreviousFriendlyTarget", PreviousFriendlyTarget) &&
                  TryGetPropertyStruct(targetTrackingC, L"PreviousHostileTarget", PreviousHostileTarget) &&
                  TryGetProperty(Pawn, L"LockOnComponent", LockOnComponent) &&
                  TryGetPropertyStruct(LockOnComponent, L"PendingLockTarget", PendingLockTarget) &&
                  TryGetPropertyStruct(LockOnComponent, L"LockedOnTargetOverride", LockedOnTargetOverride);

        if (!success) {
            LogInfo("Failed to get cockpit references, retrying next frame...");
            Pawn = nullptr;
            return false;
        }

        if (!HooksInstalled) {
            if (!AddEventHook(mechCockpit->get_class(), L"OnCalculateHeadAim", &OnCalculateHeadAim))
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

    quat GetHMDRotation() const {
        UEVR_Vector3f    pose;
        UEVR_Quaternionf rot, offset;
        const auto       hmdIndex = API::get()->param()->vr->get_hmd_index();

        API::get()->param()->vr->get_pose(hmdIndex, &pose, &rot);
        API::get()->param()->vr->get_rotation_offset(&offset);

        const quat qHmd(rot.w, rot.z, rot.x, rot.y);
        const quat qRotOffset(offset.w, offset.z, offset.x, offset.y);
        const quat combined       = normalize(qRotOffset * qHmd);
        const quat qHeadAimOffset = angleAxis(radians(*HeadAimPitchOffset), vec3(0, 1, 0));

        return normalize(combined * qHeadAimOffset);
    }

    static float QuatAngleRad(const quat& a, const quat& b) {
        const float d = clamp(abs(dot(normalize(a), normalize(b))), 0.0f, 1.0f);
        return 2.0f * acos(d);
    }

    static quat MakeYawPitchRollQuat(const float yawRad, const float pitchRad, const float rollRad) {
        const quat qYaw   = angleAxis(yawRad, vec3(0, 0, 1));
        const quat qPitch = angleAxis(pitchRad, vec3(0, 1, 0));
        const quat qRoll  = angleAxis(rollRad, vec3(1, 0, 0));
        return normalize(qYaw * qPitch * qRoll);
    }

    void ProcessHeadAim() {
        constexpr auto  fwd         = vec3(1, 0, 0);
        constexpr float deadbandRad = radians(0.15f); // 0.10–0.25
        constexpr float maxAngleRad = radians(3.0f);  // 3–5
        constexpr float lambdaSlow  = 20.0f;          // 14–20
        constexpr float lambdaFast  = 50.0f;          // 35–50

        const quat q = GetHMDRotation();

        if (!SmoothedQuatInit) {
            SmoothedHmdQuat  = q;
            SmoothedQuatInit = true;
        }

        const float ang      = QuatAngleRad(SmoothedHmdQuat, q);
        const float angEff   = max(0.0f, ang - deadbandRad);
        const float adaptive = clamp(angEff / maxAngleRad, 0.0f, 1.0f);
        const float lambda   = mix(lambdaSlow, lambdaFast, adaptive);
        const float t        = 1.0f - expf(-lambda * Delta);
        SmoothedHmdQuat      = normalize(slerp(SmoothedHmdQuat, q, t));

        const vec3 dir = normalize(SmoothedHmdQuat * fwd);

        const float yawRad   = atan2(dir.y, dir.x);
        const float pitchRad = atan2(dir.z, sqrt(dir.x * dir.x + dir.y * dir.y));

        const float yawDeg   = -degrees(yawRad);
        const float pitchDeg = -degrees(pitchRad);

        float armsTargetYawDeg   = *HeadAimLocked ? 0 : yawDeg;
        float armsTargetPitchDeg = *HeadAimLocked ? 0 : pitchDeg;

        if (!TorsoStats->Arm.IsYawUnbound)
            armsTargetYawDeg = clamp(armsTargetYawDeg, TorsoStats->Arm.BoundsLow.x, TorsoStats->Arm.BoundsHigh.x);
        if (!TorsoStats->Arm.IsPitchUnbound)
            armsTargetPitchDeg = clamp(armsTargetPitchDeg, TorsoStats->Arm.BoundsLow.y, TorsoStats->Arm.BoundsHigh.y);

        *ArmsTarget = vec2(armsTargetYawDeg, armsTargetPitchDeg);
        *HeadTarget = vec2(yawDeg, pitchDeg);
    }

    static float WrapDeg180(float a) {
        a = fmodf(a + 180.0f, 360.0f);
        if (a < 0.0f) a += 360.0f;
        return a - 180.0f;
    }

    static float DeltaAngleDeg(float current, float target) {
        return WrapDeg180(target - current);
    }

    static vec2 DeltaAngleDeg2(const vec2& current, const vec2& target) {
        return vec2(
            DeltaAngleDeg(current.x, target.x),
            DeltaAngleDeg(current.y, target.y)
        );
    }

    void ProcessArmTwist(RotationDegrees cockpitRelativeRot, RotationDegrees torsoAimRotation) {
        const float zoom        = max(0.001f, *ZoomLevel);
        const float invZoom     = 1.0f / zoom;
        const float oneMinusInv = 1.0f - invZoom;

        // Calculate the target in cockpit relative coordinates so that it's stable relative to the cockpit regardless of torso twist or pitch,
        // otherwise the target will have to chase the torso as it rotates
        vec2 targetArmRotation = *ArmsTarget * invZoom;

        static bool springInit = false;
        if (!springInit) {
            CurrentArmRotation = targetArmRotation;
            CurrentArmVelocity = vec2(0.0f);
            springInit         = true;
        }

        // Calculate velocity and new rotation
        const vec2 diff = DeltaAngleDeg2(CurrentArmRotation, targetArmRotation);
        vec2 accel = ARM_SPRING_CONSTANT * diff - ARM_DAMPING_CONSTANT * CurrentArmVelocity;
        CurrentArmVelocity += accel * Delta;
        const float maxSpeed = TorsoStats->Arm.TwistRate.x; // Just assuming x==y
        float v2 = dot(CurrentArmVelocity, CurrentArmVelocity);
        if (v2 > maxSpeed * maxSpeed) {
            CurrentArmVelocity *= maxSpeed / sqrt(v2);
        }
        CurrentArmRotation += CurrentArmVelocity * Delta;

        // Calculate aim direction
        constexpr vec3 fwd(1.0f, 0.0f, 0.0f);
        const quat qArm = MakeYawPitchRollQuat(
            -radians(CurrentArmRotation.x),
            radians(CurrentArmRotation.y),
            0.0f
        );
        const vec3 localArmTwistDir = qArm * fwd;

        const vec2 torsoDelta = *TorsoRotation - PreviousTorsoRotation;

        float yawOffset;
        float pitchOffset;

        // Kind of hacky below but this makes sure we line up with the torso aim even when the torso target is off center to the cockpit
        // torsoAimRotation is the difference between the cockpit/camera angle and the torso crosshair/aim angle
        if (*HeadAimLocked) {
            yawOffset   = torsoAimRotation.Yaw;
            pitchOffset = -torsoAimRotation.Pitch;
        } else {
            // Apply torsoDelta to catch the current frame's torso rotation and add the difference between the cockpit and torso aim accounting for zoom level
            yawOffset   = -torsoDelta.x * invZoom + torsoAimRotation.Yaw * oneMinusInv;
            pitchOffset = torsoDelta.y * invZoom - torsoAimRotation.Pitch * oneMinusInv;
        }

        // CockpitRelativeRot is the rotation of the cockpit relative to the mech
        // Add this to the offsets and create a new quaternion
        const float relativeYawRad   = radians(-cockpitRelativeRot.Yaw + yawOffset);
        const float relativePitchRad = radians(cockpitRelativeRot.Pitch + pitchOffset);
        const float relativeRollRad  = radians(-cockpitRelativeRot.Roll);
        const quat qRelative = MakeYawPitchRollQuat(relativeYawRad, relativePitchRad, relativeRollRad);

        // Rotate the arm twist direction by the relative quaternion
        const vec3 worldArmTwistDir = qRelative * localArmTwistDir;

        // Construct the yaw and pitch angles from the worldArmTwistDir
        const float yawRad   = atan2(worldArmTwistDir.y, worldArmTwistDir.x);
        const float xyLen    = sqrt(worldArmTwistDir.x * worldArmTwistDir.x + worldArmTwistDir.y * worldArmTwistDir.y);
        const float pitchRad = atan2(worldArmTwistDir.z, xyLen);
        *ArmTwist = vec2(
            -degrees(yawRad),
            -degrees(pitchRad)
        );

        // Update the previous torso rotation, used for applying the current frames' rotation to the calcs before the skeleton has been updated
        PreviousTorsoRotation = *TorsoRotation;
    }
};

// ReSharper disable once CppInconsistentNaming
std::unique_ptr<HeadAim> g_plugin{new HeadAim()}; // NOLINT(misc-use-internal-linkage)
