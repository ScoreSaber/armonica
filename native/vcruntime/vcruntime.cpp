#include <windows.h>

namespace {

using CxxFrameHandlerFn = DWORD(__cdecl*)(EXCEPTION_RECORD*, ULONG_PTR, CONTEXT*,
                                          DISPATCHER_CONTEXT*);
using ProcessingThrowFn = int*(__cdecl*)();

CxxFrameHandlerFn resolveHandler(const char* name, CxxFrameHandlerFn& cache) {
    if (cache == nullptr) {
        auto* ucrt = GetModuleHandleA("ucrtbase.dll");
        if (ucrt == nullptr) {
            ucrt = LoadLibraryA("ucrtbase.dll");
        }
        if (ucrt != nullptr) {
            cache = reinterpret_cast<CxxFrameHandlerFn>(GetProcAddress(ucrt, name));
        }
    }
    return cache;
}

DWORD invokeFrameHandler(const char* name, CxxFrameHandlerFn& cache, EXCEPTION_RECORD* record,
                         ULONG_PTR frame, CONTEXT* context, DISPATCHER_CONTEXT* dispatch) {
    auto* handler = resolveHandler(name, cache);
    if (handler == nullptr) {
        return ExceptionContinueSearch;
    }

    if (dispatch == nullptr) {
        return handler(record, frame, context, dispatch);
    }

    ULONG_PTR imageBase = 0;
    if (RtlLookupFunctionEntry(dispatch->ControlPc, &imageBase, nullptr) != nullptr) {
        return handler(record, frame, context, dispatch);
    }

    const auto savedPc = dispatch->ControlPc;
    const auto savedUnwound = dispatch->ControlPcIsUnwound;

    if (savedUnwound && savedPc >= 4 &&
        RtlLookupFunctionEntry(savedPc - 4, &imageBase, nullptr) != nullptr) {
        dispatch->ControlPc = savedPc - 4;
        dispatch->ControlPcIsUnwound = FALSE;
    } else if (dispatch->FunctionEntry != nullptr) {
        dispatch->ControlPc = dispatch->ImageBase + dispatch->FunctionEntry->BeginAddress;
        dispatch->ControlPcIsUnwound = FALSE;
    }

    const auto result = handler(record, frame, context, dispatch);
    dispatch->ControlPc = savedPc;
    dispatch->ControlPcIsUnwound = savedUnwound;
    return result;
}

CxxFrameHandlerFn cachedHandler{};
CxxFrameHandlerFn cachedHandler2{};
CxxFrameHandlerFn cachedHandler3{};

}

extern "C" {

DWORD __cdecl __CxxFrameHandler(EXCEPTION_RECORD* record, ULONG_PTR frame, CONTEXT* context,
                                DISPATCHER_CONTEXT* dispatch) {
    return invokeFrameHandler("__CxxFrameHandler", cachedHandler, record, frame, context, dispatch);
}

DWORD __cdecl __CxxFrameHandler2(EXCEPTION_RECORD* record, ULONG_PTR frame, CONTEXT* context,
                                 DISPATCHER_CONTEXT* dispatch) {
    return invokeFrameHandler("__CxxFrameHandler2", cachedHandler2, record, frame, context,
                              dispatch);
}

DWORD __cdecl __CxxFrameHandler3(EXCEPTION_RECORD* record, ULONG_PTR frame, CONTEXT* context,
                                 DISPATCHER_CONTEXT* dispatch) {
    return invokeFrameHandler("__CxxFrameHandler3", cachedHandler3, record, frame, context,
                              dispatch);
}

void __cdecl __telemetry_main_invoke_trigger(HINSTANCE) {}
void __cdecl __telemetry_main_return_trigger(HINSTANCE) {}

BOOL __cdecl __vcrt_InitializeCriticalSectionEx(CRITICAL_SECTION* cs, DWORD spinCount,
                                                DWORD flags) {
    return InitializeCriticalSectionEx(cs, spinCount, flags);
}

int __cdecl __uncaught_exceptions() {
    auto* ucrt = GetModuleHandleA("ucrtbase.dll");
    if (ucrt == nullptr) {
        ucrt = LoadLibraryA("ucrtbase.dll");
    }
    if (ucrt == nullptr) {
        return 0;
    }
    auto* fn = reinterpret_cast<ProcessingThrowFn>(GetProcAddress(ucrt, "__processing_throw"));
    return fn != nullptr ? *fn() : 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

}
