#pragma once
#include <filesystem>
#include <libloaderapi.h>

namespace Util {
    static std::filesystem::path GetModulePath() {
        static std::filesystem::path path{};
        if (!path.empty())
            return path;

        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetModulePath, &module);

        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(module, buf, MAX_PATH);
        path = std::filesystem::path(buf).parent_path();
        return path;
    }
}
