#include "pch.h"
#include "AppCore.h"
#include "sdk/Addresses.h"
#include "sdk/Game.h"
#include <atomic>

static std::atomic<bool> g_isRunning{false};

static DWORD WINAPI run(HMODULE hMod)
{
    bool ready = false;
    for (int attempts = 0; attempts < 60; ++attempts)
    {
        if (Addresses::init())
        {
            ready = true;
            break;
        }
        Sleep(250);
    }

    if (ready)
    {
        AppCore::start();

        // Wait until DELETE key is physically released before listening for eject
        while (GetAsyncKeyState(VK_DELETE) & 0x8000)
        {
            Sleep(50);
        }
        GetAsyncKeyState(VK_DELETE); // Drain any queued transition bit

        // Main engine loop
        while (!AppCore::shouldEject())
        {
            if (GetAsyncKeyState(VK_DELETE) & 0x8000)
            {
                break;
            }

            __try
            {
                AppCore::tick();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            Sleep(50);
        }

        __try
        {
            AppCore::stop();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        Sleep(150);
    }

    g_isRunning.store(false);
    FreeLibraryAndExitThread(hMod, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hMod);

        bool expected = false;
        if (!g_isRunning.compare_exchange_strong(expected, true))
        {
            return FALSE;
        }

        HANDLE t = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(run), hMod, 0, nullptr);
        if (t)
        {
            CloseHandle(t);
        }
        else
        {
            g_isRunning.store(false);
            return FALSE;
        }
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        g_isRunning.store(false);
    }
    return TRUE;
}
