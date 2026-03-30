// ReSharper disable CppDeprecatedEntity
#pragma once
#include <codecvt>
#include <locale>
#include <map>
#include <Plugin.hpp>
#include <Log.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#define MH_STATIC
#include <MinHook.h>
#include <glm/gtx/euler_angles.hpp>

using namespace uevr;
using namespace glm;

#define NATIVE_FUNCTION_OFFSET 0xD8

class PluginExtension : public Plugin {
public:
    class FFrame {
    public:
        char            Pad0[0x10];
        API::UFunction* Node;
        API::UObject*   Object;
        uint8_t*        Code;
        uint8_t*        Locals;

        template <typename T>
        T* GetParams() { return reinterpret_cast<T*>(Locals); }
    };

    using BP_FUNC = void* (*)(API::UObject*, FFrame*, void*);

    static inline PluginExtension* Instance;

    PluginExtension() {
        MH_Initialize();
    }

    virtual ~PluginExtension() override {
        RemoveAllEventHooks(false);
    }

    virtual void on_initialize() override {
        LogInfo("%s %s initialized", Name, Version);
        SetupVersionCheckHook();
        OnInitialize();
    }

    template <typename... Args>
    void LogError(const char* format, Args... args) {
        const std::string modifiedFormat = "[" + std::string(Name) + "] " + format;
        API::get()->log_error(modifiedFormat.c_str(), args...);
    }

    template <typename... Args>
    void LogWarn(const char* format, Args... args) {
        const std::string modifiedFormat = "[" + std::string(Name) + "] " + format;
        API::get()->log_warn(modifiedFormat.c_str(), args...);
    }

    template <typename... Args>
    void LogInfo(const char* format, Args... args) {
        const std::string modifiedFormat = "[" + std::string(Name) + "] " + format;
        API::get()->log_info(modifiedFormat.c_str(), args...);
    }

private:
    struct Hook {
        void**  TargetFnPtrAddress;
        BP_FUNC DetourFn;
        BP_FUNC OriginalFn;
    };

    std::map<std::wstring, Hook> Hooks;

    void SetupVersionCheckHook() {
        const auto vrGlobal = API::get()->find_uobject<API::UClass>(L"BlueprintGeneratedClass /Game/MechWarriorVR/VR_Global.VR_Global_C");
        if (!vrGlobal) {
            LogError("Failed to find VR Global class");
            return;
        }

        if (!AddEventHook(vrGlobal, VersionCheckFnName, &OnFetchPluginData)) {
            LogError("Failed to hook into %s", WideToNarrow(VersionCheckFnName).c_str());
            return;
        }
    }

    static void* OnFetchPluginData(API::UObject* vrGlobal, FFrame*, void* const) {
        if (!Instance)
            return nullptr;

        if (!vrGlobal) {
            Instance->LogError("OnFetchPluginData: vrGlobal null");
            return nullptr;
        }

        bool*    uevrLoaded = nullptr;
        int32_t* version    = nullptr;

        if (!Instance->TryGetPropertyStruct(vrGlobal, L"VersionDataSet", uevrLoaded) ||
            !Instance->TryGetPropertyStruct(vrGlobal, Instance->VersionPropertyName, version)) {
            return nullptr;
        }

        if (uevrLoaded)
            *uevrLoaded = true;
        if (version)
            *version = Instance->VersionInt;

        return nullptr;
    }

protected:
    const char*    Name                = "NOT SET";
    const char*    Version             = "0.0.0";
    int32_t        VersionInt          = 0;
    const wchar_t* VersionCheckFnName  = L"NOT SET";
    const wchar_t* VersionPropertyName = L"NOT SET";

    virtual void OnInitialize() {}

    bool AddEventHook(const API::UClass* uClass, const wchar_t* eventName, const BP_FUNC detourFn, BP_FUNC* originalFnPtr = nullptr) {
        const auto functionNameNarrow = WideToNarrow(eventName);
        LogInfo("Hooking into %s", functionNameNarrow.c_str());

        const auto function = uClass->find_function(eventName);
        if (!function) {
            LogError("Failed to find function %s", functionNameNarrow.c_str());
            return false;
        }

        const auto fName = function->get_fname()->to_string();
        LogInfo("Found function with FName %s", WideToNarrow(fName.c_str()).c_str());

        const auto targetFnPtrAddress = (void**)((uintptr_t)function + NATIVE_FUNCTION_OFFSET);
        auto&      targetFnPtrRef     = *targetFnPtrAddress;
        const auto originalFn         = (BP_FUNC)targetFnPtrRef;
        LogInfo("%s - Address: 0x%p, NatFnPtr: 0x%p, DetourPtr: 0x%p",
                functionNameNarrow.c_str(),
                targetFnPtrAddress,
                targetFnPtrRef,
                detourFn);
        targetFnPtrRef = detourFn;

        const auto hook = Hook{targetFnPtrAddress, detourFn, originalFn};
        Hooks.emplace(std::wstring(eventName), hook);

        if (originalFnPtr != nullptr) {
            *originalFnPtr = originalFn;
        }

        LogInfo("Hooked %s", functionNameNarrow.c_str());
        return true;
    }

    void RemoveEventHook(const wchar_t* eventName) {
        const auto eventNameNarrow = WideToNarrow(eventName);
        LogInfo("Removing hook for %s", eventNameNarrow.c_str());

        const auto it = Hooks.find(std::wstring(eventName));
        if (it == Hooks.end()) {
            LogError("Hook for %s does not exist", eventNameNarrow.c_str());
            return;
        }

        auto& targetFnPtrRef = *it->second.TargetFnPtrAddress;
        targetFnPtrRef       = it->second.OriginalFn;

        Hooks.erase(it);
        LogInfo("Removed hook for %s", eventNameNarrow.c_str());
    }

    void RemoveAllEventHooks(const bool log) {
        if (log) {
            LogInfo("Removing all event hooks...");
        }
        for (const auto& pair : Hooks) {
            const auto& hook           = pair.second;
            auto&       targetFnPtrRef = *hook.TargetFnPtrAddress;
            targetFnPtrRef             = hook.OriginalFn;
        }
        Hooks.clear();
        if (log) {
            LogInfo("All event hooks removed");
        }
    }

    template <typename T>
    bool TryGetProperty(const API::UObject* object, const wchar_t* propertyName, T& outProperty, const bool log = true) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::string                                propertyNameNarrowStr = converter.to_bytes(propertyName);
        const auto                                       propertyNameNarrow    = propertyNameNarrowStr.c_str();

        if (!object) {
            LogError("Failed to get property %s: invalid object", propertyNameNarrow);
            return false;
        }

        const auto data = object->get_property_data<T>(propertyName);
        if (!data) {
            LogWarn("Failed to get property %s: invalid property", propertyNameNarrow);
            return false;
        }

        outProperty = *data;
        if (log) {
            LogInfo("Found property %s", propertyNameNarrow);
        }
        return true;
    }

    template <typename T>
    bool TryGetPropertyStruct(const API::UObject* object, const wchar_t* propertyName, T*& outProperty, const bool log = true) {
        const auto propertyNameNarrow = WideToNarrow(propertyName);

        if (!object) {
            LogError("Failed to get property %s: invalid object", propertyNameNarrow.c_str());
            return false;
        }

        const auto data = object->get_property_data<T>(propertyName);
        if (!data) {
            LogWarn("Failed to get property %s: invalid property", propertyNameNarrow.c_str());
            return false;
        }

        outProperty = data;
        if (log) {
            LogInfo("Found property %s", propertyNameNarrow.c_str());
        }
        return true;
    }

    static void GetHMDPoseAndRotation(quat& outRot, vec3& outPose) {
        UEVR_Vector3f    pose;
        UEVR_Quaternionf rot, offset;
        const auto       hmdIndex = API::get()->param()->vr->get_hmd_index();

        API::get()->param()->vr->get_pose(hmdIndex, &pose, &rot);
        API::get()->param()->vr->get_rotation_offset(&offset);

        const quat qHmd(rot.w, rot.z, rot.x, rot.y);
        const quat qRotOffset(offset.w, offset.z, offset.x, offset.y);
        const quat combined = normalize(qRotOffset * qHmd);

        outRot  = normalize(combined);
        outPose = vec3(pose.x, pose.y, pose.z);
    }

    static vec3 EulerAnglesFromQuat(const quat& q) {
        const auto rot   = mat4{q};
        float      pitch = 0.0f;
        float      yaw   = 0.0f;
        float      roll  = 0.0f;
        extractEulerAngleYXZ(rot, yaw, pitch, roll);
        return {pitch, -yaw, -roll};
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

public:
    static std::string WideToNarrow(const wchar_t* wideStr) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::string                                narrowStr = converter.to_bytes(wideStr);
        return narrowStr;
    }
};
