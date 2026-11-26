#pragma once

#include <windows.h>

#include <cstdio>

namespace armonica {

inline HMODULE loadRuntimeLibrary(const char* name) {
    char directory[MAX_PATH]{};
    char path[MAX_PATH]{};
    const auto length = GetEnvironmentVariableA("ARMONICA_RUNTIME", directory, sizeof(directory));
    if (length > 0 && length < sizeof(directory) &&
        std::snprintf(path, sizeof(path), "%s\\aarch64-windows\\%s", directory, name) <
            static_cast<int>(sizeof(path))) {
        if (auto* module = LoadLibraryA(path)) {
            return module;
        }
    }
    return LoadLibraryA(name);
}

}
