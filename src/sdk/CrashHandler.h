#pragma once
#include "Logger.h"
#include <atomic>
#include <format>
#include <string>
#include <windows.h>

namespace SDK::Crash
{

inline std::atomic<uint32_t> g_lastInboundPacketId{0};
inline std::atomic<uint32_t> g_lastOutboundPacketId{0};
inline std::atomic<const char*> g_lastCheckpoint{"init"};
inline LPTOP_LEVEL_EXCEPTION_FILTER g_prevFilter = nullptr;

inline const char* getExceptionName(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
    default:                                 return "UNKNOWN_EXCEPTION";
    }
}

inline LONG WINAPI unhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    if (!pExceptionInfo || !pExceptionInfo->ExceptionRecord)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;

    // Ignore normal debugger/RPC/C++ exceptions (0xE06D7363 is MSVC C++ exception)
    if (code == 0xE06D7363 || code == 0x406D1388 || code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void* addr = pExceptionInfo->ExceptionRecord->ExceptionAddress;
    const char* name = getExceptionName(code);

    SDK::Log::log("\n=======================================================");
    SDK::Log::log("[FATAL CRASH] Exception {:#010x} ({}) @ address {}", code, name, addr);
    SDK::Log::log("[FATAL CRASH] Last Checkpoint: {}", g_lastCheckpoint.load());
    SDK::Log::log("[FATAL CRASH] Last Inbound Packet ID: {:#04x}", g_lastInboundPacketId.load());
    SDK::Log::log("[FATAL CRASH] Last Outbound Packet ID: {:#04x}", g_lastOutboundPacketId.load());

    if (code == EXCEPTION_ACCESS_VIOLATION && pExceptionInfo->ExceptionRecord->NumberParameters >= 2)
    {
        ULONG_PTR accessType = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR targetAddr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        const char* op = (accessType == 0) ? "Read" : (accessType == 1) ? "Write" : "Execute";
        SDK::Log::log("[FATAL CRASH] Details: Attempted to {} invalid memory @ {:#018x}", op, targetAddr);
    }

    // Get module containing the faulting address
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(addr),
        &hMod
    ) && hMod)
    {
        char modName[MAX_PATH] = {};
        GetModuleFileNameA(hMod, modName, sizeof(modName));
        uintptr_t rva = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(hMod);
        SDK::Log::log("[FATAL CRASH] Fault Module: {} (RVA: +{:#x})", modName, rva);
    }

    SDK::Log::log("=======================================================\n");
    SDK::Log::flush();

    return EXCEPTION_CONTINUE_SEARCH;
}

inline void install()
{
    g_prevFilter = SetUnhandledExceptionFilter(unhandledExceptionFilter);
    SDK::Log::log("[CrashHandler] Unhandled Exception Filter installed.");
}

inline void uninstall()
{
    SetUnhandledExceptionFilter(g_prevFilter);
    g_prevFilter = nullptr;
}

} // namespace SDK::Crash
