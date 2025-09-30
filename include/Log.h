#pragma once
#include <API.hpp>

namespace Log {

	template<typename... Args>
	static void LogError(const char* format, Args... args) {
	    uevr::API::get()->log_error(format, args...);
	}

	template<typename... Args>
	static void LogWarn(const char* format, Args... args) {
	    uevr::API::get()->log_warn(format, args...);
	}

	template<typename... Args>
	static void LogInfo(const char* format, Args... args) {
	    uevr::API::get()->log_info(format, args...);
	}
}
