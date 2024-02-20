// ReSharper disable CppDeprecatedEntity
#pragma once
#include <codecvt>
#include <glm/glm.hpp>
#include <locale>
#include <map>
#include <uevr/Plugin.hpp>

using namespace uevr;
using namespace glm;

#define NATIVE_FUNCTION_OFFSET 0xD8

class PluginExtension : public Plugin {
protected:
    class FFrame {
    public:
        char pad_0x0000[0x10];
        API::UFunction* node;
        API::UObject* object;
        uint8_t* code;
        uint8_t* locals;
        template <typename T> T* get_params() { return reinterpret_cast<T*>(locals); }
    };

    using BP_FUNC = void* (*)(API::UObject*, FFrame*, void*);

private:
    struct Hook {
        void** target_fn_ptr_address;
        BP_FUNC detour_fn;
        BP_FUNC original_fn;
    };

    std::map<std::wstring, Hook> hooks;

protected:
    const char* plugin_name = "NOT SET";

    bool add_event_hook(const API::UClass* klass, const wchar_t* event_name, const BP_FUNC detour_fn, BP_FUNC* original_fn_ptr = nullptr) {
        const auto function_name_narrow = wide_to_narrow(event_name);
        log_info("Hooking into %s", function_name_narrow.c_str());

        const auto function = klass->find_function(event_name);
        if (!function) {
            log_error("Failed to find function %s", function_name_narrow.c_str());
            return false;
        }

        const auto fname = function->get_fname()->to_string();
        log_info("Found function with FName %s", wide_to_narrow(fname.c_str()));

        const auto target_fn_ptr_address = (void**)((uintptr_t)function + NATIVE_FUNCTION_OFFSET);
        auto& target_fn_ptr_ref = *target_fn_ptr_address;
        const auto original_fn = (BP_FUNC)target_fn_ptr_ref;
        log_info("%s - Address: 0x%p, NatFnPtr: 0x%p, DetourPtr: 0x%p", function_name_narrow.c_str(), target_fn_ptr_address,
            target_fn_ptr_ref, detour_fn);
        target_fn_ptr_ref = detour_fn;

        log_info("Hooked %s", function_name_narrow.c_str());
        const auto hook = Hook{target_fn_ptr_address, detour_fn, original_fn};
        hooks.insert({std::wstring(event_name), hook});
        if (original_fn_ptr != nullptr) {
            *original_fn_ptr = original_fn;
        }
        return true;
    }

    void remove_event_hook(const wchar_t* event_name) {
        const auto event_name_narrow = wide_to_narrow(event_name);
        log_info("Removing hook for %s", event_name_narrow.c_str());

        const auto it = hooks.find(std::wstring(event_name));
        if (it == hooks.end()) {
            log_error("Hook for %s does not exist", event_name_narrow.c_str());
            return;
        }

        auto& target_fn_ptr_ref = *it->second.target_fn_ptr_address;
        target_fn_ptr_ref = it->second.original_fn;

        hooks.erase(it);
        log_info("Removed hook for %s", event_name_narrow.c_str());
    }

    void remove_all_event_hooks_with_log() {
        log_info("Removing all event hooks...");
        remove_all_event_hooks();
        log_info("All event hooks removed");
    }

    void remove_all_event_hooks() {
        for (const auto& pair : hooks) {
            const auto& hook = pair.second;
            auto& target_fn_ptr_ref = *hook.target_fn_ptr_address;
            target_fn_ptr_ref = hook.original_fn;
        }
        hooks.clear();
    }

    template <typename... Args> void log_error(const char* format, Args... args) {
        const std::string modified_format = "[" + std::string(plugin_name) + "] " + format;
        API::get()->log_error(modified_format.c_str(), args...);
    }

    template <typename... Args> void log_warn(const char* format, Args... args) {
        const std::string modified_format = "[" + std::string(plugin_name) + "] " + format;
        API::get()->log_warn(modified_format.c_str(), args...);
    }

    template <typename... Args> void log_info(const char* format, Args... args) {
        const std::string modified_format = "[" + std::string(plugin_name) + "] " + format;
        API::get()->log_info(modified_format.c_str(), args...);
    }

    template <typename T> bool try_get_property(const API::UObject* object, const wchar_t* property_name, T& out_property) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::string property_name_narrow_str = converter.to_bytes(property_name);
        const auto property_name_narrow = property_name_narrow_str.c_str();

        if (!object) {
            log_error("Failed to get property %s: invalid object", property_name_narrow);
            return false;
        }

        const auto data = object->get_property_data<T>(property_name);
        if (!data) {
            log_warn("Failed to get property %s: invalid property", property_name_narrow);
            return false;
        }

        out_property = *data;
        log_info("Found property %s", property_name_narrow);
        return true;
    }

    template <typename T> bool try_get_property_struct(const API::UObject* object, const wchar_t* property_name, T*& out_property) {
        const auto property_name_narrow = wide_to_narrow(property_name);

        if (!object) {
            log_error("Failed to get property %s: invalid object", property_name_narrow.c_str());
            return false;
        }

        const auto data = object->get_property_data<T>(property_name);
        if (!data) {
            log_warn("Failed to get property %s: invalid property", property_name_narrow.c_str());
            return false;
        }

        out_property = data;
        log_info("Found property %s", property_name_narrow.c_str());
        return true;
    }

    static std::string wide_to_narrow(const wchar_t* wide_str) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::string narrow_str = converter.to_bytes(wide_str);
        return narrow_str;
    }
};