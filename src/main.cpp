#include "PowermateManager.h"
#include "trayIcon.h"
#include <windows.h>
#include <iostream>

TrayIcon trayIcon;

void InitConsole() {
    if (AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        std::cout << "[Debug] Console Initialized\n";
    }
}

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR cmdLine, int) {

    HANDLE hMutex = CreateMutex(NULL, TRUE, L"UniqueAppMutexName");

         // Exit if the app is already running
        if (!hMutex) {
            std::cerr << "Failed to create mutex" << std::endl;
            return -1;
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "Application is already running" << std::endl;
            CloseHandle(hMutex);
            return 0;
        }

    // Check if -debug
    if (wcsstr(cmdLine, L"-debug") != nullptr) {
        InitConsole();
    }

    HWND hwnd = trayIcon.CreateTrayWindow(hInstance);  // Create tray window
    if (!hwnd) {
        std::cerr << "[Error] Failed to create tray window\n";
        CloseHandle(hMutex);
        return -1;
    }

    PowermateManager::FindAndOpenDevice();
    trayIcon.InitTrayIcon(hwnd);

    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&trayIcon));

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (msg.message == WM_CLOSE) {
        // Stop App 
        PowermateManager::Stop();
    }

    CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}