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
		template<typename T>
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
	    LogInfo("%s %s initialized", PluginName, PluginVersion);
	    OnInitialize();
	}

	template<typename... Args>
	void LogError(const char* format, Args... args) {
		const std::string modifiedFormat = "[" + std::string(PluginName) + "] " + format;
		API::get()->log_error(modifiedFormat.c_str(), args...);
	}

	template<typename... Args>
	void LogWarn(const char* format, Args... args) {
		const std::string modifiedFormat = "[" + std::string(PluginName) + "] " + format;
		API::get()->log_warn(modifiedFormat.c_str(), args...);
	}

	template<typename... Args>
	void LogInfo(const char* format, Args... args) {
		const std::string modifiedFormat = "[" + std::string(PluginName) + "] " + format;
		API::get()->log_info(modifiedFormat.c_str(), args...);
	}

private:
	struct Hook {
		void**  TargetFnPtrAddress;
		BP_FUNC DetourFn;
		BP_FUNC OriginalFn;
	};

	std::map<std::wstring, Hook> Hooks;

protected:
	const char* PluginName = "NOT SET";
    const char* PluginVersion = "0.0.0";

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

		LogInfo("Hooked %s", functionNameNarrow.c_str());
		const auto hook = Hook{targetFnPtrAddress, detourFn, originalFn};
		Hooks.insert({std::wstring(eventName), hook});
		if (originalFnPtr != nullptr) {
			*originalFnPtr = originalFn;
		}
		return true;
	}

	void RemoveEventHook(const wchar_t* event_name) {
		const auto eventNameNarrow = WideToNarrow(event_name);
		LogInfo("Removing hook for %s", eventNameNarrow.c_str());

		const auto it = Hooks.find(std::wstring(event_name));
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

	template<typename T>
	bool TryGetProperty(const API::UObject* object, const wchar_t* propertyName, T& out_property) {
		std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
		const std::string                                 propertyNameNarrowStr = converter.to_bytes(propertyName);
		const auto                                        propertyNameNarrow    = propertyNameNarrowStr.c_str();

		if (!object) {
			LogError("Failed to get property %s: invalid object", propertyNameNarrow);
			return false;
		}

		const auto data = object->get_property_data<T>(propertyName);
		if (!data) {
			LogWarn("Failed to get property %s: invalid property", propertyNameNarrow);
			return false;
		}

		out_property = *data;
		LogInfo("Found property %s", propertyNameNarrow);
		return true;
	}

	template<typename T>
	bool TryGetPropertyStruct(const API::UObject* object, const wchar_t* propertyName, T*& outProperty) {
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
		LogInfo("Found property %s", propertyNameNarrow.c_str());
		return true;
	}

public:
	static std::string WideToNarrow(const wchar_t* wide_str) {
		std::wstring_convert<std::codecvt_utf8<wchar_t> > converter;
		const std::string                                 narrowStr = converter.to_bytes(wide_str);
		return narrowStr;
	}
};
