#include "steam_api.hpp"

#include "common/runtime.hpp"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* clientDllName = "lsteamclient_a64.dll";
constexpr const char* clientInterface = "SteamClient021";

using CreateInterfaceFn = void*(__cdecl*)(const char*, int*);
using GetCallbackFn = bool(__cdecl*)(HSteamPipe, CallbackMsg_t*, int32*);
using FreeCallbackFn = bool(__cdecl*)(HSteamPipe);
using GetCallResultFn = bool(__cdecl*)(HSteamPipe, SteamAPICall_t, void*, int, int, bool*);
using ReleaseThreadMemoryFn = void(__cdecl*)(int);

struct ScopedLock {
    CRITICAL_SECTION& cs;
    explicit ScopedLock(CRITICAL_SECTION& section) : cs(section) { EnterCriticalSection(&cs); }
    ~ScopedLock() { LeaveCriticalSection(&cs); }
};

template <typename T> struct SmallList {
    T* data{};
    std::size_t count{};

    void push(const T& item) {
        auto* grown = static_cast<T*>(std::realloc(data, (count + 1) * sizeof(T)));
        if (grown == nullptr) {
            return;
        }
        data = grown;
        data[count++] = item;
    }

    void clear() {
        std::free(data);
        data = nullptr;
        count = 0;
    }
};

struct ClientApi {
    HMODULE module{};
    CreateInterfaceFn createInterface{};
    GetCallbackFn getCallback{};
    FreeCallbackFn freeCallback{};
    GetCallResultFn getCallResult{};
    ReleaseThreadMemoryFn releaseThreadMemory{};
};

struct InterfaceCacheEntry {
    char* version;
    void* iface;
};

struct MSVCCallbackBase {
    void** vtable;
    std::uint8_t flags;
    int callbackId;
};

struct CallbackEntry {
    std::uint64_t key;
    MSVCCallbackBase* callback;
};

constexpr std::uint8_t callbackFlagRegistered = 0x01;
constexpr std::uint8_t callbackFlagGameServer = 0x02;

CRITICAL_SECTION stateLock{};
ClientApi clientApi{};
ISteamClient* steamClient{};
HSteamPipe steamPipe{};
HSteamUser steamUser{};
bool manualDispatch{};
std::uintptr_t contextCounter = 1;
SmallList<InterfaceCacheEntry> userInterfaces{};
SmallList<CallbackEntry> callbacks{};
SmallList<CallbackEntry> callResults{};

template <typename Fn> bool loadFunction(HMODULE module, const char* name, Fn& out) {
    out = reinterpret_cast<Fn>(GetProcAddress(module, name));
    return out != nullptr;
}

void writeError(SteamErrMsg* out, const char* message) {
    if (out != nullptr) {
        std::snprintf(*out, sizeof(*out), "%s", message);
    }
}

bool ensureClientApi() {
    if (clientApi.module != nullptr) {
        return true;
    }

    auto* module = armonica::loadRuntimeLibrary(clientDllName);
    if (module == nullptr) {
        return false;
    }

    ClientApi loaded{.module = module};
    const bool ok = loadFunction(module, "CreateInterface", loaded.createInterface) &&
                    loadFunction(module, "Steam_BGetCallback", loaded.getCallback) &&
                    loadFunction(module, "Steam_FreeLastCallback", loaded.freeCallback) &&
                    loadFunction(module, "Steam_GetAPICallResult", loaded.getCallResult);
    loadFunction(module, "Steam_ReleaseThreadLocalMemory", loaded.releaseThreadMemory);
    if (!ok) {
        FreeLibrary(module);
        return false;
    }

    clientApi = loaded;
    return true;
}

void ensureAppIdEnv() {
    char value[32]{};
    if (GetEnvironmentVariableA("SteamAppId", value, sizeof(value)) > 0) {
        return;
    }

    FILE* file = std::fopen("steam_appid.txt", "r");
    if (file == nullptr) {
        return;
    }

    unsigned int appId = 0;
    if (std::fscanf(file, "%u", &appId) == 1 && appId != 0) {
        std::snprintf(value, sizeof(value), "%u", appId);
        SetEnvironmentVariableA("SteamAppId", value);
        SetEnvironmentVariableA("SteamGameId", value);
    }
    std::fclose(file);
}

bool readRegistryValue(const char* subkey, const char* name, DWORD type, void* data, DWORD size) {
    HKEY key{};
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD actualType = 0;
    const auto status =
        RegQueryValueExA(key, name, nullptr, &actualType, static_cast<BYTE*>(data), &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && actualType == type;
}

std::size_t versionPrefixLength(const char* version) {
    auto length = std::strlen(version);
    while (length > 0 && version[length - 1] >= '0' && version[length - 1] <= '9') {
        --length;
    }
    return length;
}

void* findOrCreateUserInterface(HSteamUser user, const char* version) {
    if (steamClient == nullptr || version == nullptr) {
        return nullptr;
    }

    ScopedLock lock(stateLock);
    for (std::size_t i = 0; i < userInterfaces.count; ++i) {
        if (std::strcmp(userInterfaces.data[i].version, version) == 0) {
            return userInterfaces.data[i].iface;
        }
    }

    auto* iface = armonica::rawGenericInterface(steamClient, user, steamPipe, version);
    if (iface != nullptr) {
        userInterfaces.push({_strdup(version), iface});
    }
    return iface;
}

std::size_t takeCallbacks(SmallList<CallbackEntry>& list, bool matchKey, std::uint64_t key,
                          MSVCCallbackBase* matchCallback, MSVCCallbackBase*** taken) {
    std::size_t kept = 0;
    std::size_t found = 0;
    auto* out = taken != nullptr ? static_cast<MSVCCallbackBase**>(
                                       std::malloc((list.count + 1) * sizeof(MSVCCallbackBase*)))
                                 : nullptr;

    for (std::size_t i = 0; i < list.count; ++i) {
        const auto entry = list.data[i];
        if ((!matchKey || entry.key == key) &&
            (matchCallback == nullptr || entry.callback == matchCallback)) {
            if (out != nullptr) {
                out[found] = entry.callback;
            }
            ++found;
        } else {
            list.data[kept++] = entry;
        }
    }
    list.count = kept;
    if (taken != nullptr) {
        *taken = out;
    }
    return found;
}

void dispatchCallResult(HSteamPipe pipe, const SteamAPICallCompleted_t* completed) {
    using GetSizeFn = int (*)(void*);
    using RunResultFn = void (*)(void*, void*, bool, SteamAPICall_t);

    MSVCCallbackBase** targets = nullptr;
    std::size_t count = 0;
    {
        ScopedLock lock(stateLock);
        count = takeCallbacks(callResults, true, completed->m_hAsyncCall, nullptr, &targets);
    }

    for (std::size_t i = 0; targets != nullptr && i < count; ++i) {
        auto* cb = targets[i];
        const int size = reinterpret_cast<GetSizeFn>(cb->vtable[2])(cb);
        void* buffer = std::calloc(1, size > 0 ? static_cast<std::size_t>(size) : 1);
        bool failed = false;
        if (buffer == nullptr ||
            !SteamAPI_ManualDispatch_GetAPICallResult(pipe, completed->m_hAsyncCall, buffer, size,
                                                      cb->callbackId, &failed)) {
            failed = true;
        }
        reinterpret_cast<RunResultFn>(cb->vtable[0])(cb, buffer, failed, completed->m_hAsyncCall);
        std::free(buffer);
    }
    std::free(targets);
}

}

namespace armonica {

const char* mapInterfaceVersion(const char* version) {
    if (version == nullptr) {
        return nullptr;
    }
    const auto prefix = versionPrefixLength(version);
    if (prefix == 0 || version[prefix] == '\0') {
        return version;
    }
    for (auto* current = interfaceVersions; *current != nullptr; ++current) {
        if (versionPrefixLength(*current) != prefix ||
            std::strncmp(*current, version, prefix) != 0) {
            continue;
        }
        if (std::atoi(version + prefix) >= std::atoi(*current + prefix)) {
            return version;
        }
        return *current;
    }
    return version;
}

void* legacyUserInterface(const char* version) {
    return findOrCreateUserInterface(steamUser, version);
}

}

S_API ISteamClient* g_pSteamClientGameServer = nullptr;

S_API ESteamAPIInitResult S_CALLTYPE SteamInternal_SteamAPI_Init(
    const char* pszInternalCheckInterfaceVersions, SteamErrMsg* pOutErrMsg) {
    ScopedLock lock(stateLock);
    if (steamClient != nullptr && steamUser != 0) {
        return k_ESteamAPIInitResult_OK;
    }

    ensureAppIdEnv();
    if (!ensureClientApi()) {
        writeError(pOutErrMsg, "failed to load lsteamclient_a64.dll");
        return k_ESteamAPIInitResult_NoSteamClient;
    }

    steamClient = static_cast<ISteamClient*>(clientApi.createInterface(clientInterface, nullptr));
    if (steamClient == nullptr) {
        writeError(pOutErrMsg, "SteamClient021 unavailable");
        return k_ESteamAPIInitResult_VersionMismatch;
    }

    steamPipe = SteamAPI_ISteamClient_CreateSteamPipe(steamClient);
    if (steamPipe == 0) {
        writeError(pOutErrMsg, "failed to create Steam pipe");
        return k_ESteamAPIInitResult_NoSteamClient;
    }

    steamUser = SteamAPI_ISteamClient_ConnectToGlobalUser(steamClient, steamPipe);
    if (steamUser == 0) {
        SteamAPI_ISteamClient_BReleaseSteamPipe(steamClient, steamPipe);
        steamPipe = 0;
        writeError(pOutErrMsg, "failed to connect to Steam user");
        return k_ESteamAPIInitResult_NoSteamClient;
    }

    for (const char* ver = pszInternalCheckInterfaceVersions; ver != nullptr && *ver != '\0';
         ver += std::strlen(ver) + 1) {
        if (SteamAPI_ISteamClient_GetISteamGenericInterface(steamClient, steamUser, steamPipe,
                                                            ver) == nullptr) {
            char msg[256]{};
            std::snprintf(msg, sizeof(msg), "missing interface %s", ver);
            writeError(pOutErrMsg, msg);
            SteamAPI_ISteamClient_ReleaseUser(steamClient, steamPipe, steamUser);
            SteamAPI_ISteamClient_BReleaseSteamPipe(steamClient, steamPipe);
            steamPipe = 0;
            steamUser = 0;
            return k_ESteamAPIInitResult_VersionMismatch;
        }
    }

    ++contextCounter;
    if (pOutErrMsg != nullptr) {
        (*pOutErrMsg)[0] = '\0';
    }
    return k_ESteamAPIInitResult_OK;
}

S_API ESteamAPIInitResult S_CALLTYPE SteamAPI_InitFlat(SteamErrMsg* pOutErrMsg) {
    return SteamInternal_SteamAPI_Init(nullptr, pOutErrMsg);
}

S_API bool S_CALLTYPE SteamAPI_InitSafe() {
    return SteamInternal_SteamAPI_Init(nullptr, nullptr) == k_ESteamAPIInitResult_OK;
}

extern "C" __declspec(dllexport) bool S_CALLTYPE legacySteamAPIInit() __asm__("SteamAPI_Init");
extern "C" bool S_CALLTYPE legacySteamAPIInit() { return SteamAPI_InitSafe(); }

S_API bool S_CALLTYPE SteamAPI_InitAnonymousUser() { return SteamAPI_InitSafe(); }

S_API void S_CALLTYPE SteamAPI_Shutdown() {
    ScopedLock lock(stateLock);
    if (steamClient != nullptr) {
        if (steamPipe != 0 && steamUser != 0) {
            SteamAPI_ISteamClient_ReleaseUser(steamClient, steamPipe, steamUser);
        }
        if (steamPipe != 0) {
            SteamAPI_ISteamClient_BReleaseSteamPipe(steamClient, steamPipe);
        }
        SteamAPI_ISteamClient_BShutdownIfAllPipesClosed(steamClient);
    }
    steamPipe = 0;
    steamUser = 0;
    steamClient = nullptr;
    for (std::size_t i = 0; i < userInterfaces.count; ++i) {
        std::free(userInterfaces.data[i].version);
    }
    userInterfaces.clear();
    ++contextCounter;
}

S_API bool S_CALLTYPE SteamAPI_RestartAppIfNecessary(uint32) { return false; }

S_API void S_CALLTYPE SteamAPI_ReleaseCurrentThreadMemory() {
    if (clientApi.releaseThreadMemory != nullptr) {
        clientApi.releaseThreadMemory(0);
    }
}

S_API void S_CALLTYPE SteamAPI_WriteMiniDump(uint32, void*, uint32) {}
S_API void S_CALLTYPE SteamAPI_SetMiniDumpComment(const char*) {}
S_API void S_CALLTYPE SteamAPI_UseBreakpadCrashHandler(const char*, const char*, const char*, bool,
                                                       void*, PFNPreMinidumpCallback) {}
S_API void S_CALLTYPE SteamAPI_SetBreakpadAppID(uint32) {}
S_API void SteamAPI_SetTryCatchCallbacks(bool) {}

S_API bool S_CALLTYPE SteamAPI_IsSteamRunning() {
    if (steamPipe != 0) {
        return true;
    }
    DWORD pid = 0;
    if (!readRegistryValue("Software\\Valve\\Steam\\ActiveProcess", "pid", REG_DWORD, &pid,
                           sizeof(pid)) ||
        pid == 0) {
        return false;
    }
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (process == nullptr) {
        return false;
    }
    const bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);
    return running;
}

S_API const char* SteamAPI_GetSteamInstallPath() {
    static char path[MAX_PATH]{};
    if (path[0] == '\0' &&
        !readRegistryValue("Software\\Valve\\Steam", "SteamPath", REG_SZ, path, sizeof(path) - 1)) {
        path[0] = '\0';
    }
    return path;
}

S_API HSteamPipe S_CALLTYPE SteamAPI_GetHSteamPipe() { return steamPipe; }
S_API HSteamUser S_CALLTYPE SteamAPI_GetHSteamUser() { return steamUser; }
extern "C" __declspec(dllexport) HSteamPipe GetHSteamPipe() { return steamPipe; }
extern "C" __declspec(dllexport) HSteamUser GetHSteamUser() { return steamUser; }
S_API ISteamClient* SteamClient() { return steamClient; }

S_API void* S_CALLTYPE SteamInternal_CreateInterface(const char* ver) {
    ver = armonica::mapInterfaceVersion(ver);
    if (ver == nullptr) {
        return nullptr;
    }
    if (std::strncmp(ver, "SteamClient", 11) == 0) {
        return ensureClientApi() ? clientApi.createInterface(ver, nullptr) : nullptr;
    }
    if (steamClient == nullptr) {
        return nullptr;
    }
    return SteamAPI_ISteamClient_GetISteamGenericInterface(steamClient, steamUser, steamPipe, ver);
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateUserInterface(HSteamUser user,
                                                               const char* version) {
    return findOrCreateUserInterface(user, armonica::mapInterfaceVersion(version));
}

S_API void* SteamAPI_ISteamClient_GetISteamAppList(ISteamClient* self, HSteamUser user,
                                                   HSteamPipe pipe, const char* version) {
    return armonica::rawGenericInterface(self, user, pipe, version);
}

S_API void* S_CALLTYPE SteamInternal_ContextInit(void* contextInitData) {
    struct ContextInitData {
        void (*init)(void*);
        std::uintptr_t counter;
        void* context[1];
    };
    auto* data = static_cast<ContextInitData*>(contextInitData);
    if (data->counter != contextCounter) {
        ScopedLock lock(stateLock);
        if (data->counter != contextCounter) {
            data->init(&data->context);
            data->counter = contextCounter;
        }
    }
    return &data->context;
}

S_API void S_CALLTYPE SteamAPI_ManualDispatch_Init() { manualDispatch = true; }
S_API void S_CALLTYPE SteamAPI_ManualDispatch_RunFrame(HSteamPipe) {}

S_API bool S_CALLTYPE SteamAPI_ManualDispatch_GetNextCallback(HSteamPipe pipe, CallbackMsg_t* msg) {
    if (clientApi.getCallback == nullptr) {
        return false;
    }
    int32 ignored = 0;
    return clientApi.getCallback(pipe, msg, &ignored);
}

S_API void S_CALLTYPE SteamAPI_ManualDispatch_FreeLastCallback(HSteamPipe pipe) {
    if (clientApi.freeCallback != nullptr) {
        clientApi.freeCallback(pipe);
    }
}

S_API bool S_CALLTYPE SteamAPI_ManualDispatch_GetAPICallResult(HSteamPipe pipe, SteamAPICall_t call,
                                                               void* callback, int size,
                                                               int expected, bool* failed) {
    if (clientApi.getCallResult == nullptr) {
        return false;
    }
    return clientApi.getCallResult(pipe, call, callback, size, expected, failed);
}

S_API void S_CALLTYPE SteamAPI_RegisterCallback(CCallbackBase* callback, int id) {
    auto* cb = reinterpret_cast<MSVCCallbackBase*>(callback);
    ScopedLock lock(stateLock);
    cb->flags |= callbackFlagRegistered;
    cb->callbackId = id;
    callbacks.push({static_cast<std::uint64_t>(static_cast<std::int64_t>(id)), cb});
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback(CCallbackBase* callback) {
    auto* cb = reinterpret_cast<MSVCCallbackBase*>(callback);
    ScopedLock lock(stateLock);
    takeCallbacks(callbacks, false, 0, cb, nullptr);
    cb->flags &= ~callbackFlagRegistered;
}

S_API void S_CALLTYPE SteamAPI_RegisterCallResult(CCallbackBase* callback, SteamAPICall_t call) {
    ScopedLock lock(stateLock);
    callResults.push({call, reinterpret_cast<MSVCCallbackBase*>(callback)});
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallResult(CCallbackBase* callback, SteamAPICall_t call) {
    ScopedLock lock(stateLock);
    takeCallbacks(callResults, true, call, reinterpret_cast<MSVCCallbackBase*>(callback), nullptr);
}

S_API void S_CALLTYPE SteamAPI_RunCallbacks() {
    using RunCallbackFn = void (*)(void*, void*);

    if (steamPipe == 0 || manualDispatch) {
        return;
    }

    CallbackMsg_t msg{};
    while (SteamAPI_ManualDispatch_GetNextCallback(steamPipe, &msg)) {
        if (msg.m_iCallback == SteamAPICallCompleted_t::k_iCallback) {
            dispatchCallResult(steamPipe,
                               reinterpret_cast<const SteamAPICallCompleted_t*>(msg.m_pubParam));
        } else {
            MSVCCallbackBase** targets = nullptr;
            std::size_t count = 0;
            {
                ScopedLock lock(stateLock);
                targets = static_cast<MSVCCallbackBase**>(
                    std::malloc((callbacks.count + 1) * sizeof(MSVCCallbackBase*)));
                const auto key =
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(msg.m_iCallback));
                for (std::size_t i = 0; targets != nullptr && i < callbacks.count; ++i) {
                    const auto entry = callbacks.data[i];
                    if (entry.key == key && (entry.callback->flags & callbackFlagGameServer) == 0) {
                        targets[count++] = entry.callback;
                    }
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                reinterpret_cast<RunCallbackFn>(targets[i]->vtable[1])(targets[i], msg.m_pubParam);
            }
            std::free(targets);
        }
        SteamAPI_ManualDispatch_FreeLastCallback(steamPipe);
    }
}

S_API ESteamAPIInitResult S_CALLTYPE SteamInternal_GameServer_Init_V2(uint32, uint16, uint16,
                                                                      EServerMode, const char*,
                                                                      const char*,
                                                                      SteamErrMsg* err) {
    writeError(err, "game servers unsupported");
    return k_ESteamAPIInitResult_FailedGeneric;
}

S_API bool S_CALLTYPE SteamInternal_GameServer_Init(uint32, uint16, uint16, uint16, EServerMode,
                                                    const char*) {
    return false;
}

extern "C" __declspec(dllexport) bool SteamGameServer_InitSafe(uint32, uint16, uint16, uint16,
                                                               EServerMode, const char*) {
    return false;
}

S_API void S_CALLTYPE SteamGameServer_Shutdown() {}
S_API void S_CALLTYPE SteamGameServer_RunCallbacks() {}
S_API bool SteamGameServer_BSecure() { return false; }
S_API uint64 SteamGameServer_GetSteamID() { return 0; }
S_API HSteamPipe S_CALLTYPE SteamGameServer_GetHSteamPipe() { return 0; }
S_API HSteamUser S_CALLTYPE SteamGameServer_GetHSteamUser() { return 0; }
extern "C" __declspec(dllexport) uint32 SteamGameServer_GetIPCCallCount() { return 0; }
S_API void* S_CALLTYPE SteamInternal_FindOrCreateGameServerInterface(HSteamUser, const char*) {
    return nullptr;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&stateLock);
        armonica::patchWineFPCR();
    }
    return TRUE;
}
