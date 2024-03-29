#include <PluginExtension.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

class HeadAim final : public PluginExtension {
    enum HeadAimMode : int { DISABLED, ARMS_ONLY, TORSO_AND_ARMS };

    API::UObject* pawn = nullptr;
    HeadAimMode* head_aim_mode = nullptr;
    vec2* arm_twist = nullptr;
    vec3* target_view_rotator = nullptr; // Pitch, yaw, roll
    vec3* last_view_rotator = nullptr;
    bool* armlock_enabled = nullptr;
    bool* enable_arm_lock = nullptr;
    float* zoom_level = nullptr;
    float* head_aim_pitch_offset = nullptr;
    float* head_aim_yaw_offset = nullptr;
    vec3* head_crosshair_rotator = nullptr; // Pitch, yaw, roll
    vec2* torso_rotation = nullptr;
    vec2* arm_twist_bounds_high = nullptr;
    vec2* arm_twist_bounds_low = nullptr;
    vec2* arm_twist_rate = nullptr;
    API::UObject* vr_headcrosshairrotator = nullptr;
    vec3* mech_relative_rotation = nullptr;
    vec2* head_target = nullptr;
    vec2* arms_target = nullptr;
    bool* head_aim_locked = nullptr;
    void** current_target = nullptr;
    void** previous_friendly_target = nullptr;
    void** previous_hostile_target = nullptr;
    void** pending_lock_target = nullptr;
    void** locked_on_target_override = nullptr;

    bool in_mech = false;
    float delta = 0;
    vec2 rotation_speed = vec2(0, 0);

#define ARM_SPRING_CONSTANT 175.0f
#define ARM_DAMPING_CONSTANT 25.0f

    vec2 current_velocity = vec2();
    vec2 current_rotation = vec2();
    vec2 smoothed_head_rotation = vec2();

    struct TwistBounds {
        // Yaw, Pitch
        vec2 twist_rate;
        bool is_yaw_unbound;
        bool is_pitch_unbound;
        vec2 bounds_low;
        vec2 bounds_high;
    };

    struct TorsoStats {
        TwistBounds torso;
        TwistBounds arm;
    };

    TorsoStats* torso_stats = nullptr;

public:
    inline static HeadAim* instance;

    HeadAim() {
        instance = this;
        plugin_name = "HeadAim";
    }

    ~HeadAim() override { remove_all_event_hooks(); }

    void on_pre_engine_tick(API::UGameEngine* engine, const float delta) override {
        if (const auto active_pawn = API::get()->get_local_pawn(0); active_pawn != pawn) {
            in_mech = on_new_pawn(active_pawn);
        }

        this->delta = delta;

        if (in_mech && *head_aim_mode == ARMS_ONLY) {
            // LastRelativeViewRotator is used by the targeting logic, setting this here doesn't seem to affect anything else but targeting
            *last_view_rotator = vec3(head_target->y + torso_rotation->y, head_target->x + torso_rotation->x, 0.0f);
        }
    }

    static void* on_torso_twist(API::UObject*, FFrame*, void* const) {
        instance->update_head_aim();
        return nullptr;
    }

    static void* on_set_target(API::UObject*, FFrame* frame, void* const) {
        *instance->current_target = *frame->get_params<void**>();
        *instance->previous_friendly_target = nullptr;
        *instance->previous_hostile_target = nullptr;
        *instance->pending_lock_target = nullptr;
        *instance->locked_on_target_override = nullptr;
        return nullptr;
    }

private:
    bool on_new_pawn(API::UObject* active_pawn) {
        if (pawn) {
            remove_all_event_hooks_with_log();
        }

        pawn = active_pawn;
        if (!active_pawn)
            return false;

        API::UObject *mech_view_c, *mech_mesh_c, *cockpit_c, *torso_twist_c, *mech_cockpit, *user_settings, *vr_hudmanager, *mech_root_c,
            *target_tracking_c, *lock_on_c;

        const auto player_controller = API::get()->get_player_controller(0);

        bool success =
            try_get_property(pawn, L"MechViewComponent", mech_view_c) && try_get_property(pawn, L"MechMeshComponent", mech_mesh_c);

        if (!success) {
            log_info("Failed to get Mech references - probably not in a Mech");
            return false;
        }

        success = try_get_property(mech_mesh_c, L"FirstPersonCockpitComponent", cockpit_c) &&
                  try_get_property(cockpit_c, L"ChildActor", mech_cockpit) &&
                  try_get_property_struct(mech_cockpit, L"HeadAimMode", head_aim_mode) &&
                  try_get_property(pawn, L"TorsoTwistComponent", torso_twist_c) &&
                  try_get_property_struct(torso_twist_c, L"bArmlockEnabled", armlock_enabled) &&
                  try_get_property_struct(torso_twist_c, L"ArmTwist", arm_twist) &&
                  try_get_property_struct(mech_view_c, L"TargetRelativeViewRotator", target_view_rotator) &&
                  try_get_property_struct(mech_view_c, L"LastRelativeViewRotator", last_view_rotator) &&
                  try_get_property_struct(torso_twist_c, L"TorsoStats", torso_stats) &&
                  try_get_property(player_controller, L"MWGameUserSettings", user_settings) &&
                  try_get_property_struct(user_settings, L"EnableArmLock", enable_arm_lock) &&
                  try_get_property(mech_cockpit, L"VR_HUDManager", vr_hudmanager) &&
                  try_get_property_struct(vr_hudmanager, L"ZoomLevel", zoom_level) &&
                  try_get_property(mech_cockpit, L"VR_HeadCrosshairRotator", vr_headcrosshairrotator) &&
                  try_get_property_struct(vr_headcrosshairrotator, L"RelativeRotation", head_crosshair_rotator) &&
                  try_get_property_struct(torso_twist_c, L"TorsoTwist", torso_rotation) &&
                  try_get_property(pawn, L"RootComponent", mech_root_c) &&
                  try_get_property_struct(mech_root_c, L"RelativeRotation", mech_relative_rotation) &&
                  try_get_property_struct(mech_cockpit, L"HeadAimPitchOffset", head_aim_pitch_offset) &&
                  try_get_property_struct(mech_cockpit, L"HeadAimYawOffset", head_aim_yaw_offset) &&
                  try_get_property_struct(mech_cockpit, L"HeadTarget", head_target) &&
                  try_get_property_struct(mech_cockpit, L"ArmsTarget", arms_target) &&
                  try_get_property_struct(mech_cockpit, L"HeadAimLocked", head_aim_locked) &&
                  try_get_property(pawn, L"TargetTracking", target_tracking_c) &&
                  try_get_property_struct(target_tracking_c, L"CurrentTarget", current_target) &&
                  try_get_property_struct(target_tracking_c, L"PreviousFriendlyTarget", previous_friendly_target) &&
                  try_get_property_struct(target_tracking_c, L"PreviousHostileTarget", previous_hostile_target) &&
                  try_get_property(pawn, L"LockOnComponent", lock_on_c) &&
                  try_get_property_struct(lock_on_c, L"PendingLockTarget", pending_lock_target) &&
                  try_get_property_struct(lock_on_c, L"LockedOnTargetOverride", locked_on_target_override);

        if (!success) {
            log_info("Failed to get cockpit references, retrying next frame...");
            pawn = nullptr;
            return false;
        }

        if (!add_event_hook(mech_cockpit->get_class(), L"OnTorsoTwist", &on_torso_twist))
            return false;

        if (!add_event_hook(mech_cockpit->get_class(), L"OnSetTarget", &on_set_target))
            return false;

        log_info("Mech references found and hooks enabled");
        return true;
    }

    static vec3 euler_angles_from_quat(const quat& q) {
        const auto rot = mat4{q};
        float pitch = 0.0f;
        float yaw = 0.0f;
        float roll = 0.0f;
        extractEulerAngleYXZ(rot, yaw, pitch, roll);
        return {pitch, -yaw, -roll};
    }

    vec3 get_hmd_rotation() const {
        UEVR_Vector3f pose;
        UEVR_Quaternionf rot, offset;
        const auto hmd_index = API::get()->param()->vr->get_hmd_index();

        API::get()->param()->vr->get_pose(hmd_index, &pose, &rot);
        API::get()->param()->vr->get_rotation_offset(&offset);

        const quat quat_rot(rot.w, rot.x, rot.y, rot.z);
        const quat quat_rot_offset(offset.w, offset.x, offset.y, offset.z);
        const quat combined_rot = normalize(quat_rot_offset * quat_rot);
        vec3 euler = degrees(euler_angles_from_quat(combined_rot));
        euler.x += *head_aim_pitch_offset;
        euler.y += *head_aim_yaw_offset;

        return euler;
    }

    void update_head_aim() {
        switch (*head_aim_mode) {
        case ARMS_ONLY:
            arms_only();
            break;
        case TORSO_AND_ARMS:
            torso_and_arms();
            break;
        case DISABLED:
        default:
            break;
        }
    }

    float recenter_hack_offset = 0.000001f;

    void arms_only() {
        const vec3 hmd_rotation = get_hmd_rotation();

        update_head_aim_target({hmd_rotation.y, hmd_rotation.x});

        vec2 target_rotation;
        if (*armlock_enabled == *enable_arm_lock) {
            *head_aim_locked = false;
            target_rotation = *arms_target;
        } else {
            target_rotation = vec2(0, 0);
            *arms_target = vec2(0, 0);
            *head_aim_locked = true;
        }

        target_rotation /= *zoom_level;

        const vec2 rotation_diff = target_rotation - current_rotation;
        const vec2 rotation_rate = torso_stats->arm.twist_rate;

        const vec2 force = ARM_SPRING_CONSTANT * rotation_diff - ARM_DAMPING_CONSTANT * current_velocity;
        current_velocity += force * delta;
        current_velocity = clamp(current_velocity, -rotation_rate, rotation_rate);
        current_rotation += current_velocity * delta;

        current_rotation = {clamp(current_rotation.x, torso_stats->arm.bounds_low.x, torso_stats->arm.bounds_high.x),
            clamp(current_rotation.y, torso_stats->arm.bounds_low.y, torso_stats->arm.bounds_high.y)};

        *arm_twist = current_rotation + *torso_rotation;

        // Imperceptable alternating jitter added to torso rotation to stop game's C++ from locking the arm target to center once stable
        torso_rotation->x = torso_rotation->x + recenter_hack_offset;
        recenter_hack_offset *= -1.0f;
    }

    void update_head_aim_target(const vec2& target_head_rotation) {
        constexpr float max_distance = 2.0f;
        constexpr float min_interp = 0.05f;
        constexpr float max_interp = 1.0f;

        const float distance = length(target_head_rotation - smoothed_head_rotation);
        const float smoothing_factor = min_interp + min((distance / max_distance), 1.0f) * (max_interp - min_interp);

        smoothed_head_rotation = lerp(smoothed_head_rotation, target_head_rotation, smoothing_factor);
        *head_target = smoothed_head_rotation;
        *arms_target = {clamp(head_target->x, torso_stats->arm.bounds_low.x, torso_stats->arm.bounds_high.x),
            clamp(head_target->y, torso_stats->arm.bounds_low.y, torso_stats->arm.bounds_high.y)};
    }

    void torso_and_arms() const {
        const auto hmd_rotation = get_hmd_rotation();
        target_view_rotator->x = hmd_rotation.x + torso_rotation->y;
        target_view_rotator->y = hmd_rotation.y + torso_rotation->x;
        target_view_rotator->z = 0;
    }
};

std::unique_ptr<HeadAim> g_plugin{new HeadAim()};