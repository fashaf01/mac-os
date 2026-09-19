#include "core/Log.h"
#include "core/Version.h"
#include "dock/DockWindow.h"
#include "render/GraphicsDevice.h"

#include <windows.h>
#include <objbase.h>

namespace {

constexpr const wchar_t* kSingleInstanceMutex = L"Local\\MacDock.SingleInstance";

// Politely hands focus to the dock already running, instead of starting a
// second one that would fight it for the screen edge.
bool alreadyRunning(HANDLE& mutexOut) {
    mutexOut = CreateMutexW(nullptr, TRUE, kSingleInstanceMutex);
    return mutexOut && GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HANDLE instanceMutex = nullptr;
    if (alreadyRunning(instanceMutex)) {
        if (instanceMutex) CloseHandle(instanceMutex);
        return 0;
    }

    md::log::init();
    MD_LOG(L"MacDock %s starting", md::kVersion);

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(comHr)) {
        MD_LOG(L"CoInitializeEx failed: 0x%08X", comHr);
        md::log::shutdown();
        if (instanceMutex) CloseHandle(instanceMutex);
        return 1;
    }

    int exitCode = 0;
    {
        md::GraphicsDevice graphics;
        md::DockWindow dock;

        if (!graphics.create()) {
            MD_LOG(L"graphics initialisation failed");
            MessageBoxW(nullptr,
                        L"MacDock could not initialise Direct3D.\n\n"
                        L"This usually means the graphics driver needs updating.",
                        L"MacDock", MB_OK | MB_ICONERROR);
            exitCode = 1;
        } else if (!dock.initialize(&graphics)) {
            MD_LOG(L"dock initialisation failed");
            exitCode = 1;
        } else {
            // Blocking message loop. With no animation in flight there is no
            // timer, so the thread sleeps in GetMessage and the process uses
            // no CPU at all.
            MSG msg{};
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            exitCode = static_cast<int>(msg.wParam);
        }

        dock.shutdown();
        graphics.destroy();
    }

    CoUninitialize();
    MD_LOG(L"MacDock exiting with code %d", exitCode);
    md::log::shutdown();

    if (instanceMutex) CloseHandle(instanceMutex);
    return exitCode;
}
