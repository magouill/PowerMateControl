#include "trayIcon.h"
#include "PowermateManager.h"
#include "ProfileManager.h"
#include "resource.h"
#include <tchar.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <iostream>
#include <hidsdi.h>

// Constructor to initialize custom icons
TrayIcon::TrayIcon() {
    deviceIcons[true]  = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON_CONNECTED));  // Device connected icon
    deviceIcons[false] = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON_DISCONNECTED));  // Device disconnected icon
}

// Destructor: Cleaning up
TrayIcon::~TrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyIcon(deviceIcons[true]);
    DestroyIcon(deviceIcons[false]);
    if (hDevNotify) {
        UnregisterDeviceNotification(hDevNotify);
    }
    if (hMenu) {
        DestroyMenu(hMenu);
    }
}

// Function to create the tray message-only window
HWND TrayIcon::CreateTrayWindow(HINSTANCE hInstance) {
    const TCHAR CLASS_NAME[] = _T("PowermateTrayWindow");
    WNDCLASS wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);
    hwndTray = CreateWindowEx(0, CLASS_NAME, _T(""), 0,
                              0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);
    SetWindowLongPtr(hwndTray, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Setup device notification for HID devices
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = {};
    NotificationFilter.dbcc_size = sizeof(NotificationFilter);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HidD_GetHidGuid(&NotificationFilter.dbcc_classguid);

    hDevNotify = RegisterDeviceNotification(
        hwndTray,
        &NotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    return hwndTray;
}

// Function to initialize the tray icon and menu
void TrayIcon::InitTrayIcon(HWND hwnd) {
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_USER + 1;
    Shell_NotifyIcon(NIM_ADD, &nid); // Add the tray icon to the system tray
    hMenu = CreatePopupMenu();
    UpdateTrayIcon();
}

// Function to update tray icon based on device status
void TrayIcon::UpdateTrayIcon() {
    bool isConnected = PowermateManager::IsConnected();
    nid.hIcon = deviceIcons[isConnected];
    wcscpy_s(nid.szTip, isConnected ? L"Powermate Connected" : L"Powermate Disconnected");
    Shell_NotifyIcon(NIM_MODIFY, &nid);
    PopulateTrayMenu();
}

// Function to populate the tray menu
void TrayIcon::PopulateTrayMenu() {
    if (hMenu != NULL) {
        while (DeleteMenu(hMenu, 0, MF_BYPOSITION)) {}
    }

    // Add device status
    AppendMenu(hMenu, MF_STRING | MF_GRAYED, 0, PowermateManager::IsConnected() ? L"Powermate connected" : L"Powermate disconnected");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    // Add profile entries
    const std::vector<std::wstring>& profiles = cachedProfiles;
    size_t currentProfile = ProfileManager::GetCurrentProfileIndex();
    for (size_t i = 0; i < profiles.size(); ++i) {
        UINT flags = MF_STRING;
        if (i == currentProfile) {
            flags |= MF_CHECKED;
        }
        AppendMenu(hMenu, flags, ID_TRAY_PROFILE_BASE + i, profiles[i].c_str());
    }
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Add "Run at startup" checkbox
    AppendMenuW(hMenu, MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : 0), ID_TRAY_AUTOSTART, L"Run at startup");
    if (IsAutoStartEnabled() && WasDisabledByWindows()) { 
        // Add note if startup is blocked by Windows
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Disabled in Windows Startup settings");
    }
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    // Add Exit
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
}

// Function to handle tray message
LRESULT CALLBACK TrayIcon::TrayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto* trayIcon = reinterpret_cast<TrayIcon*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!trayIcon)
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    switch (uMsg) {
        case WM_USER + 1: { // Tray icon interaction
            if (lParam == WM_RBUTTONUP) {
                trayIcon->UpdateTrayIcon();
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(trayIcon->hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            }
            return 0;
        }

        case WM_COMMAND: { // Tray menu selection
            if (LOWORD(wParam) == ID_TRAY_EXIT)
                PostQuitMessage(0);
            else
                trayIcon->HandleTrayMenuSelection(wParam);
            return 0;
        }

        case WM_DEVICECHANGE: { // Device plugged/unplugged
            PowermateManager::HandleDeviceChange(wParam);
            trayIcon->UpdateTrayIcon();
            return 0;
        }
        
        case WM_POWERBROADCAST: { // System suspend/resume
            if (wParam == PBT_APMSUSPEND || wParam == PBT_APMRESUMESUSPEND) {
                PowermateManager::HandleDeviceChange(wParam);
                trayIcon->UpdateTrayIcon();
            }
            return TRUE;
        }

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// Function to handle tray menu selection
void TrayIcon::HandleTrayMenuSelection(WPARAM wParam) {
    int id = LOWORD(wParam);
    if (id >= ID_TRAY_PROFILE_BASE && id < ID_TRAY_PROFILE_BASE + (int)cachedProfiles.size()) {
        ProfileManager::SetCurrentProfile(id - ID_TRAY_PROFILE_BASE);
        PopulateTrayMenu();
    } else if (id == ID_TRAY_AUTOSTART) {
        ToggleAutoStart();
        CheckMenuItem(hMenu, ID_TRAY_AUTOSTART, IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED);
    }
}

// Run at Startup
bool TrayIcon::IsAutoStartEnabled() {
    const wchar_t* appName = L"PowerMateControl";
    HKEY hRunKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_READ, &hRunKey) != ERROR_SUCCESS) {
        return false;
    }
    
    DWORD type = 0;
    wchar_t value[MAX_PATH];
    DWORD size = sizeof(value);
    LONG runResult = RegQueryValueExW(hRunKey, appName, nullptr, &type, (LPBYTE)value, &size);
    RegCloseKey(hRunKey);

    return runResult == ERROR_SUCCESS && type == REG_SZ;
}

// Function to handle toggling the "Run at Startup"
void TrayIcon::ToggleAutoStart() {
    const wchar_t* appName = L"PowerMateControl";
    wchar_t appPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, appPath, MAX_PATH) == 0) return;

    HKEY hRunKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_WRITE, &hRunKey) != ERROR_SUCCESS) return;

    if (IsAutoStartEnabled()) {
        std::wcout << L"[Debug] Disabling Run at Startup\n";
        RegDeleteValueW(hRunKey, appName); // Disable
    } else {
        std::wcout << L"[Debug] Enabling Run at Startup with path: " << appPath << std::endl;
        RegSetValueExW(hRunKey, appName, 0, REG_SZ, (BYTE*)appPath, (DWORD)((wcslen(appPath) + 1) * sizeof(wchar_t))); // Enable
    }

    RegCloseKey(hRunKey);
}

// Check if the App is disabled by Windows settings
bool TrayIcon::WasDisabledByWindows() {
    BYTE binaryStatus[12] = {};
    DWORD dwSize = sizeof(binaryStatus);
    HKEY hApprovedKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, approvedKey, 0, KEY_READ, &hApprovedKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    bool isDisabled = false;
    DWORD type = 0;
    if (RegQueryValueExW(hApprovedKey, L"PowerMateControl", nullptr, &type, binaryStatus, &dwSize) == ERROR_SUCCESS) {
        isDisabled = (binaryStatus[0] == 0x03);  // Disabled by user/Windows
    }

    RegCloseKey(hApprovedKey);
    return isDisabled;
}
