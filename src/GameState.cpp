#include "GameState.h"
#include "Log.h"
#include <windows.h>
#include <atomic>
#include <memory>
#include <PatternScanner.h>
#include <polyhook2/Detour/x86Detour.hpp>
#include <polyhook2/CapstoneDisassembler.hpp>

namespace {
std::atomic<unsigned long> g_gameGlobal{0};
std::unique_ptr<PLH::x86Detour> g_constructHook, g_destroyHook;
uint64_t g_origConstruct{}, g_origDestroy{};

unsigned long __stdcall hookConstruct(unsigned long ddGame) {
    using Fn = unsigned long(__stdcall*)(unsigned long);
    auto result = reinterpret_cast<Fn>(g_origConstruct)(ddGame);
    __try {
        const auto value = *reinterpret_cast<unsigned long*>(ddGame + 0x488);
        g_gameGlobal.store(value);
        Log::write("ConstructGameGlobal: ddGame=0x%08lX gameGlobal=0x%08lX", ddGame, value);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { g_gameGlobal.store(0); }
    return result;
}
unsigned long __fastcall hookDestroy(int self, int edx) {
    Log::write("DestroyGameGlobal: self=0x%08X; leaving match", self);
    g_gameGlobal.store(0);
    using Fn = unsigned long(__fastcall*)(int,int);
    return reinterpret_cast<Fn>(g_origDestroy)(self,edx);
}
}
bool GameState::install() {
    try {
        uintptr_t construct = hl::FindPatternMask(
            "\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x24\x53\x55\x8B\x6C\x24\x3C\x8B\x85\x00\x00\x00\x00\x8B\x48\x24",
            "???????xx????xxxx????xxxxxxxxxxx????xxx");
        uintptr_t destroy = hl::FindPatternMask(
            "\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x51\x56\x8B\xF1\x89\x74\x24\x04\xC7\x06\x00\x00\x00\x00\x8B\xC6",
            "???????xx????xxxx????xxxxxxxxxx????xxx");
        // Verified W:A 3.8.1.0 offsets. The executable may be relocated by ASLR,
        // therefore convert the canonical 0x400000 image addresses to RVAs.
        const uintptr_t exeBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!construct) construct = exeBase + (0x526500u - 0x400000u);
        if (!destroy) destroy = exeBase + (0x56DFB0u - 0x400000u);
        Log::write("GameState patterns: construct=0x%08llX destroy=0x%08llX",
            static_cast<unsigned long long>(construct), static_cast<unsigned long long>(destroy));
        if (!construct || !destroy) {
            Log::write("GameState install failed: one or more patterns not found");
            return false;
        }
        static PLH::CapstoneDisassembler dis(PLH::Mode::x86);
        g_constructHook=std::make_unique<PLH::x86Detour>(construct,(uint64_t)&hookConstruct,reinterpret_cast<uint64_t*>(&g_origConstruct),dis);
        g_destroyHook=std::make_unique<PLH::x86Detour>(destroy,(uint64_t)&hookDestroy,reinterpret_cast<uint64_t*>(&g_origDestroy),dis);
        const bool constructOk = g_constructHook->hook();
        const bool destroyOk = constructOk && g_destroyHook->hook();
        Log::write("GameState hook result: construct=%d destroy=%d", constructOk ? 1 : 0, destroyOk ? 1 : 0);
        return constructOk && destroyOk;
    } catch (const std::exception& e) {
        Log::write("GameState install exception: %s", e.what());
        return false;
    } catch (...) {
        Log::write("GameState install exception: unknown");
        return false;
    }
}
bool GameState::inMatch() { return g_gameGlobal.load()!=0; }
