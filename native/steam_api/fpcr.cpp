#include "steam_api.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint32_t buggySequence[3] = {0xd53b4401u, 0xd53b4422u, 0xb3407c41u};
constexpr std::uint32_t fixedBfi = 0xb3607c41u;
constexpr std::uintptr_t pageSize = 4096;
constexpr long long nrMprotect = 226;
constexpr long long protRx = 0x5;
constexpr long long protRwx = 0x7;

long long linuxMprotect(void* address, std::uint64_t length, long long prot) {
    register long long x8 __asm__("x8") = nrMprotect;
    register long long x0 __asm__("x0") = reinterpret_cast<long long>(address);
    register long long x1 __asm__("x1") = static_cast<long long>(length);
    register long long x2 __asm__("x2") = prot;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
    return x0;
}

}

namespace armonica {

void patchWineFPCR() {
    auto** dispatcher = reinterpret_cast<void**>(
        GetProcAddress(GetModuleHandleA("ntdll"), "__wine_syscall_dispatcher"));
    if (dispatcher == nullptr || *dispatcher == nullptr) {
        return;
    }

    std::uint32_t code[1024]{};
    auto* base = static_cast<char*>(*dispatcher) - sizeof(code);
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), base, code, sizeof(code), &read)) {
        return;
    }

    char* target = nullptr;
    int matches = 0;
    for (std::size_t i = 0; i + 3 <= sizeof(code) / sizeof(code[0]); ++i) {
        if (std::memcmp(&code[i], buggySequence, sizeof(buggySequence)) == 0) {
            target = base + (i + 2) * sizeof(std::uint32_t);
            ++matches;
        }
    }
    if (matches != 1) {
        return;
    }

    auto* page = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(target) & ~(pageSize - 1));
    if (linuxMprotect(page, pageSize, protRwx) != 0) {
        return;
    }
    *reinterpret_cast<volatile std::uint32_t*>(target) = fixedBfi;
    linuxMprotect(page, pageSize, protRx);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(std::uint32_t));

    std::uint64_t fpcr = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr &= ~3ull;
    __asm__ volatile("msr fpcr, %0" :: "r"(fpcr));
}

}
