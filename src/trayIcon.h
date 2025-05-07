// Header file for TrayIcon class
#pragma once

#include "PowermateManager.h"
#include "ProfileManager.h"
#include <map>
#include <dbt.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <comdef.h>
#include <objbase.h>

class TrayIcon {
private:
    NOTIFYICONDATA nid;
    HMENU hMenu;
    HWND hwndTray;
    HDEVNOTIFY hDevNotify;
    std::map<bool, HICON> deviceIcons;
    std::vector<std::wstring> cachedProfiles = ProfileManager::GetProfileList();

public:
    static constexpr UINT ID_TRAY_EXIT = 10000;
    static constexpr UINT ID_TRAY_AUTOSTART = 4001;
    static constexpr const wchar_t* runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const wchar_t* approvedKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
    static constexpr UINT ID_TRAY_PROFILE_BASE = 100;

    TrayIcon();
    ~TrayIcon();
    HWND CreateTrayWindow(HINSTANCE hInstance);
    void InitTrayIcon(HWND hwnd);
    void UpdateTrayIcon();
    void PopulateTrayMenu();
    void HandleTrayMenuSelection(WPARAM wParam);
    static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void ToggleAutoStart();
    static bool IsAutoStartEnabled();
    static bool WasDisabledByWindows();
};
