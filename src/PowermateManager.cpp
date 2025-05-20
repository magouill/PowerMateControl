#include "PowermateManager.h"
#include "TriggerAction.h"
#include <hidsdi.h>
#include <setupapi.h>
#include <iostream>
#include <atomic>
#include <vector>
#include <dbt.h>
#include <mutex>
#include <thread>

// Static variable definitions
std::atomic<bool> PowermateManager::running(false);
std::atomic<bool> PowermateManager::connected(false);
std::atomic<HANDLE> PowermateManager::hDevice{ INVALID_HANDLE_VALUE };
std::thread PowermateManager::inputThread;
std::mutex PowermateManager::deviceMutex;

// Find Powermate device Path
bool PowermateManager::FindPowerMateDevicePath(std::wstring& out) {
    GUID g; HidD_GetHidGuid(&g);
    HDEVINFO h = SetupDiGetClassDevs(&g, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (h == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA d = { sizeof(d) };
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(h, nullptr, &g, i, &d); ++i) {
        DWORD sz = 0;
        SetupDiGetDeviceInterfaceDetail(h, &d, nullptr, 0, &sz, nullptr);
        if (!sz) continue;

        std::vector<BYTE> b(sz);
        auto p = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(b.data());
        p->cbSize = sizeof(*p);

        if (SetupDiGetDeviceInterfaceDetail(h, &d, p, sz, nullptr, nullptr)) {
            std::wstring s = p->DevicePath;
            if (s.find(L"vid_077d") != std::wstring::npos && s.find(L"pid_0410") != std::wstring::npos) {
                out = s;
                SetupDiDestroyDeviceInfoList(h);
                return true;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(h);
    return false;
}

// Check if device is connected
bool PowermateManager::IsConnected() {
    return connected.load() && hDevice.load() != INVALID_HANDLE_VALUE;
}

// Find and open the device
bool PowermateManager::FindAndOpenDevice() {
    std::wstring path;
    if (!FindPowerMateDevicePath(path)) {
        std::cerr << "[Debug] Powermate device not found\n";
        return false;
    }

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::cerr << "[Debug] Failed to open Powermate\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(deviceMutex);
        hDevice.store(h);
        connected.store(true);
    }

    std::cerr << "[Debug] Powermate device connected\n";

    return true;
}

// Start reading inputs
void PowermateManager::StartReading() {
    std::lock_guard<std::mutex> lock(deviceMutex);

    if (running.load()) return;
    if (!IsConnected()) return;

    running.store(true);
    inputThread = std::thread(&PowermateManager::InputLoop);
}

// Handle device change (arrival/removal)
void PowermateManager::HandleDeviceChange(WPARAM wParam) {
    std::wstring path;

    if (wParam == DBT_DEVICEARRIVAL && FindAndOpenDevice()) {
        StartReading();
        return;
    } else if (wParam == DBT_DEVICEREMOVECOMPLETE) {
        if (FindPowerMateDevicePath(path)) {
            std::wcerr << L"[Debug] Powermate is still connected\n";
        } else {
            std::wcerr << L"[Debug] Powermate is no longer connected\n";
            Stop();
        }
    } else if (wParam == PBT_APMSUSPEND) {
    std::wcout << L"[Debug] Suspending, stopping device\n";
    Stop();
    }
    else if (wParam == PBT_APMRESUMESUSPEND) {
        if (!IsConnected()) {
            if (FindAndOpenDevice()) {
                std::wcout << L"[Debug] Reconnected after system resume\n";
            } else {
                connected.store(false);
                std::wcerr << L"[Debug] Failed to reconnect Powermate after resume\n";
            }
        }
    }
}

// The input reading loop
void PowermateManager::InputLoop() {
    unsigned char buffer[8] = {};
    DWORD bytesRead = 0;
    bool buttonDown = false;
    int backoffMs = 1000;

    while (running.load()) {
        if (!IsConnected()) {
            if (FindAndOpenDevice()) {
                std::cout << "[Info] Device reconnected\n";
                backoffMs = 1000;
            } else {
                std::cerr << "[Debug] Waiting for device...\n";
                Sleep(backoffMs);
                backoffMs = (backoffMs * 2 < 10000) ? backoffMs * 2 : 10000;
                continue;
            }
        }

        HANDLE h;
        {
            std::lock_guard<std::mutex> lock(deviceMutex);
            h = hDevice.load();
        }

        if (h == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        if (!ReadFile(h, buffer, sizeof(buffer), &bytesRead, nullptr)) {
            DWORD err = GetLastError();
            std::cerr << "[Error] ReadFile failed: " << err << "\n";

            if (err == ERROR_DEVICE_NOT_CONNECTED || err == ERROR_INVALID_HANDLE) {
                connected.store(false);
                {
                    std::lock_guard<std::mutex> lock(deviceMutex);
                    if (hDevice.load() != INVALID_HANDLE_VALUE) {
                        CloseHandle(hDevice.load());
                        hDevice.store(INVALID_HANDLE_VALUE);
                    }
                }
                break; // Exit thread on device removal
            }

            connected.store(false);
            {
                std::lock_guard<std::mutex> lock(deviceMutex);
                if (hDevice.load() != INVALID_HANDLE_VALUE) {
                    CloseHandle(hDevice.load());
                    hDevice.store(INVALID_HANDLE_VALUE);
                }
            }
            break;
        }

        if (bytesRead < 3) continue;

        int8_t rotation = static_cast<int8_t>(buffer[2]);
        if (rotation != 0) {
            std::cout << (rotation < 0 ? "ROTATE RIGHT" : "ROTATE LEFT") << std::endl;
            HandleInput(rotation < 0 ? PowermateInputType::ROTATE_RIGHT : PowermateInputType::ROTATE_LEFT);
        }

        bool isPressed = buffer[1] == 1;
        if (isPressed != buttonDown) {
            std::cout << (isPressed ? "BUTTON PRESSED" : "BUTTON RELEASED") << "\n";
            buttonDown = isPressed;
            if (!isPressed) {
                HandleInput(PowermateInputType::BUTTON_RELEASE);
            }
        }
    }

    running.store(false);
}


// Forward input to TriggerAction handler
void PowermateManager::HandleInput(PowermateInputType inputType) {
    TriggerAction::HandleAction(inputType);
}

// Stop reading inputs and close device
void PowermateManager::Stop() {
    running.store(false);

    if (inputThread.joinable()) {
        inputThread.join();
    }

    CloseDevice();
}

// Close device handle and mark as invalid
void PowermateManager::CloseDevice() {
    std::lock_guard<std::mutex> lock(deviceMutex);
    HANDLE device = hDevice.load();
    if (device != INVALID_HANDLE_VALUE) {
        CloseHandle(device);
        hDevice.store(INVALID_HANDLE_VALUE);
    }
    connected.store(false);
}