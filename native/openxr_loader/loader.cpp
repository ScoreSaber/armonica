#include "common/runtime.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using XrResult = std::int32_t;
using XrVersion = std::uint64_t;
using XrInstance = void*;
using XrVoidFn = void (*)();

constexpr XrResult xrSuccess = 0;
constexpr XrResult xrErrorValidationFailure = -1;
constexpr XrResult xrErrorInitializationFailed = -6;
constexpr XrResult xrErrorFunctionUnsupported = -7;
constexpr XrResult xrErrorHandleInvalid = -12;
constexpr XrResult xrErrorApiLayerNotPresent = -36;

constexpr std::uint32_t structTypeLoaderInfo = 1;
constexpr std::uint32_t structTypeRuntimeRequest = 3;
constexpr std::uint32_t loaderInfoVersion = 1;
constexpr std::uint32_t runtimeRequestVersion = 1;
constexpr std::uint32_t loaderRuntimeInterfaceVersion = 1;

constexpr XrVersion makeVersion(std::uint16_t major, std::uint16_t minor, std::uint32_t patch) {
    return (static_cast<XrVersion>(major) << 48) | (static_cast<XrVersion>(minor) << 32) | patch;
}

using GetInstanceProcAddrFn = XrResult(WINAPI*)(XrInstance, const char*, XrVoidFn*);
using EnumerateExtensionsFn = XrResult(WINAPI*)(const char*, std::uint32_t, std::uint32_t*, void*);
using CreateInstanceFn = XrResult(WINAPI*)(const void*, XrInstance*);

struct XrNegotiateLoaderInfo {
    std::uint32_t structType;
    std::uint32_t structVersion;
    std::size_t structSize;
    std::uint32_t minInterfaceVersion;
    std::uint32_t maxInterfaceVersion;
    XrVersion minApiVersion;
    XrVersion maxApiVersion;
};

struct XrNegotiateRuntimeRequest {
    std::uint32_t structType;
    std::uint32_t structVersion;
    std::size_t structSize;
    std::uint32_t runtimeInterfaceVersion;
    XrVersion runtimeApiVersion;
    GetInstanceProcAddrFn getInstanceProcAddr;
};

using NegotiateRuntimeFn = XrResult(WINAPI*)(const XrNegotiateLoaderInfo*,
                                             XrNegotiateRuntimeRequest*);

constexpr const char* runtimeDllName = "wineopenxr_a64.dll";

using Argument = std::uintptr_t;
using ForwardedFn = XrResult(WINAPI*)(Argument, Argument, Argument, Argument, Argument, Argument,
                                      Argument, Argument);

XrResult WINAPI noInstance(Argument, Argument, Argument, Argument, Argument, Argument, Argument,
                           Argument) {
    return xrErrorHandleInvalid;
}

#define XR_RUNTIME_FUNCTIONS(X)                                                                    \
    X(xrAcquireSwapchainImage)                                                                     \
    X(xrApplyHapticFeedback)                                                                       \
    X(xrAttachSessionActionSets)                                                                   \
    X(xrBeginFrame)                                                                                \
    X(xrBeginSession)                                                                              \
    X(xrCreateAction)                                                                              \
    X(xrCreateActionSet)                                                                           \
    X(xrCreateActionSpace)                                                                         \
    X(xrCreateReferenceSpace)                                                                      \
    X(xrCreateSession)                                                                             \
    X(xrCreateSwapchain)                                                                           \
    X(xrDestroyAction)                                                                             \
    X(xrDestroyActionSet)                                                                          \
    X(xrDestroyInstance)                                                                           \
    X(xrDestroySession)                                                                            \
    X(xrDestroySpace)                                                                              \
    X(xrDestroySwapchain)                                                                          \
    X(xrEndFrame)                                                                                  \
    X(xrEndSession)                                                                                \
    X(xrEnumerateBoundSourcesForAction)                                                            \
    X(xrEnumerateEnvironmentBlendModes)                                                            \
    X(xrEnumerateReferenceSpaces)                                                                  \
    X(xrEnumerateSwapchainFormats)                                                                 \
    X(xrEnumerateSwapchainImages)                                                                  \
    X(xrEnumerateViewConfigurationViews)                                                           \
    X(xrEnumerateViewConfigurations)                                                               \
    X(xrGetActionStateBoolean)                                                                     \
    X(xrGetActionStateFloat)                                                                       \
    X(xrGetActionStatePose)                                                                        \
    X(xrGetActionStateVector2f)                                                                    \
    X(xrGetCurrentInteractionProfile)                                                              \
    X(xrGetInputSourceLocalizedName)                                                               \
    X(xrGetInstanceProperties)                                                                     \
    X(xrGetReferenceSpaceBoundsRect)                                                               \
    X(xrGetSystem)                                                                                 \
    X(xrGetSystemProperties)                                                                       \
    X(xrGetViewConfigurationProperties)                                                            \
    X(xrLocateSpace)                                                                               \
    X(xrLocateSpaces)                                                                              \
    X(xrLocateViews)                                                                               \
    X(xrPathToString)                                                                              \
    X(xrPollEvent)                                                                                 \
    X(xrReleaseSwapchainImage)                                                                     \
    X(xrRequestExitSession)                                                                        \
    X(xrResultToString)                                                                            \
    X(xrStopHapticFeedback)                                                                        \
    X(xrStringToPath)                                                                              \
    X(xrStructureTypeToString)                                                                     \
    X(xrSuggestInteractionProfileBindings)                                                         \
    X(xrSyncActions)                                                                               \
    X(xrWaitFrame)                                                                                 \
    X(xrWaitSwapchainImage)

#define DECLARE_SLOT(name) ForwardedFn slot_##name = noInstance;
XR_RUNTIME_FUNCTIONS(DECLARE_SLOT)

struct Forward {
    const char* name;
    ForwardedFn* slot;
};

#define LIST_SLOT(name) {#name, &slot_##name},
constexpr Forward forwards[] = {XR_RUNTIME_FUNCTIONS(LIST_SLOT)};

CRITICAL_SECTION initLock{};
HMODULE runtimeModule{};
GetInstanceProcAddrFn runtimeGetProcAddr{};

struct ScopedLock {
    CRITICAL_SECTION& cs;
    explicit ScopedLock(CRITICAL_SECTION& section) : cs(section) { EnterCriticalSection(&cs); }
    ~ScopedLock() { LeaveCriticalSection(&cs); }
};

bool ensureRuntime() {
    ScopedLock lock(initLock);
    if (runtimeGetProcAddr != nullptr) {
        return true;
    }

    runtimeModule = armonica::loadRuntimeLibrary(runtimeDllName);
    if (runtimeModule == nullptr) {
        return false;
    }

    auto negotiate = reinterpret_cast<NegotiateRuntimeFn>(
        GetProcAddress(runtimeModule, "xrNegotiateLoaderRuntimeInterface"));
    if (negotiate == nullptr) {
        return false;
    }

    const XrNegotiateLoaderInfo loaderInfo{
        .structType = structTypeLoaderInfo,
        .structVersion = loaderInfoVersion,
        .structSize = sizeof(XrNegotiateLoaderInfo),
        .minInterfaceVersion = loaderRuntimeInterfaceVersion,
        .maxInterfaceVersion = loaderRuntimeInterfaceVersion,
        .minApiVersion = makeVersion(1, 0, 0),
        .maxApiVersion = makeVersion(1, 0x3ff, 0xffffffff),
    };
    XrNegotiateRuntimeRequest request{
        .structType = structTypeRuntimeRequest,
        .structVersion = runtimeRequestVersion,
        .structSize = sizeof(XrNegotiateRuntimeRequest),
        .runtimeInterfaceVersion = 0,
        .runtimeApiVersion = 0,
        .getInstanceProcAddr = nullptr,
    };

    if (negotiate(&loaderInfo, &request) != xrSuccess || request.getInstanceProcAddr == nullptr) {
        return false;
    }

    runtimeGetProcAddr = request.getInstanceProcAddr;
    return true;
}

}

extern "C" {

__declspec(dllexport) XrResult WINAPI xrInitializeLoaderKHR(const void*) { return xrSuccess; }

__declspec(dllexport) XrResult WINAPI xrEnumerateApiLayerProperties(std::uint32_t,
                                                                    std::uint32_t* countOutput,
                                                                    void*) {
    if (countOutput == nullptr) {
        return xrErrorValidationFailure;
    }
    *countOutput = 0;
    return xrSuccess;
}

__declspec(dllexport) XrResult WINAPI
xrEnumerateInstanceExtensionProperties(const char* layerName, std::uint32_t capacityInput,
                                       std::uint32_t* countOutput, void* properties) {
    if (layerName != nullptr && layerName[0] != '\0') {
        return xrErrorApiLayerNotPresent;
    }
    if (!ensureRuntime()) {
        return xrErrorInitializationFailed;
    }
    XrVoidFn fn = nullptr;
    const auto res = runtimeGetProcAddr(nullptr, "xrEnumerateInstanceExtensionProperties", &fn);
    if (res != xrSuccess || fn == nullptr) {
        return res != xrSuccess ? res : xrErrorFunctionUnsupported;
    }
    return reinterpret_cast<EnumerateExtensionsFn>(fn)(nullptr, capacityInput, countOutput,
                                                       properties);
}

__declspec(dllexport) XrResult WINAPI xrCreateInstance(const void* createInfo,
                                                       XrInstance* instance) {
    if (!ensureRuntime()) {
        return xrErrorInitializationFailed;
    }
    XrVoidFn fn = nullptr;
    const auto res = runtimeGetProcAddr(nullptr, "xrCreateInstance", &fn);
    if (res != xrSuccess || fn == nullptr) {
        return res != xrSuccess ? res : xrErrorFunctionUnsupported;
    }
    const auto created = reinterpret_cast<CreateInstanceFn>(fn)(createInfo, instance);
    if (created != xrSuccess) {
        return created;
    }

    for (const auto& forward : forwards) {
        XrVoidFn resolved = nullptr;
        if (runtimeGetProcAddr(*instance, forward.name, &resolved) == xrSuccess &&
            resolved != nullptr) {
            *forward.slot = reinterpret_cast<ForwardedFn>(resolved);
        }
    }
    return created;
}

#define EXPORT_FORWARD(name)                                                                       \
    __declspec(dllexport) XrResult WINAPI name(Argument a, Argument b, Argument c, Argument d,     \
                                               Argument e, Argument f, Argument g, Argument h) {   \
        return slot_##name(a, b, c, d, e, f, g, h);                                                \
    }
XR_RUNTIME_FUNCTIONS(EXPORT_FORWARD)

__declspec(dllexport) XrResult WINAPI xrGetInstanceProcAddr(XrInstance instance, const char* name,
                                                            XrVoidFn* function) {
    if (name == nullptr || function == nullptr) {
        return xrErrorValidationFailure;
    }
    *function = nullptr;

    if (std::strcmp(name, "xrGetInstanceProcAddr") == 0) {
        *function = reinterpret_cast<XrVoidFn>(&xrGetInstanceProcAddr);
        return xrSuccess;
    }
    if (std::strcmp(name, "xrEnumerateApiLayerProperties") == 0) {
        *function = reinterpret_cast<XrVoidFn>(&xrEnumerateApiLayerProperties);
        return xrSuccess;
    }
    if (std::strcmp(name, "xrEnumerateInstanceExtensionProperties") == 0) {
        *function = reinterpret_cast<XrVoidFn>(&xrEnumerateInstanceExtensionProperties);
        return xrSuccess;
    }
    if (std::strcmp(name, "xrCreateInstance") == 0) {
        *function = reinterpret_cast<XrVoidFn>(&xrCreateInstance);
        return xrSuccess;
    }
    if (std::strcmp(name, "xrInitializeLoaderKHR") == 0) {
        *function = reinterpret_cast<XrVoidFn>(&xrInitializeLoaderKHR);
        return xrSuccess;
    }

    if (!ensureRuntime()) {
        return xrErrorInitializationFailed;
    }
    return runtimeGetProcAddr(instance, name, function);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&initLock);
    }
    return TRUE;
}
}
