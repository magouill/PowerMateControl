#include "PowermateManager.h"
#include "TriggerAction.h"
#include <hidsdi.h>
#include <setupapi.h>
#include <iostream>
#include <atomic>
#include <vector>
#include <dbt.h>
#include <mutex>

std::atomic<bool> PowermateManager::running(false);
std::atomic<bool> PowermateManager::connected(false);
std::atomic<HANDLE> PowermateManager::hDevice{ INVALID_HANDLE_VALUE };
std::thread PowermateManager::inputThread;
std::mutex PowermateManager::deviceMutex;

DWORD PowermateManager::pressStartTime = 0;
bool PowermateManager::isLongPress = false;
const DWORD PowermateManager::LONG_PRESS_THRESHOLD = 1000;

// Find Powermate device Path
bool PowermateManager::FindPowerMateDevicePath(std::wstring& out) {
    GUID g; HidD_GetHidGuid(&g);
    HDEVINFO h = SetupDiGetClassDevs(&g, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (h == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA d = { sizeof(d) };
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(h, 0, &g, i, &d); ++i) {
        DWORD sz = 0;
        SetupDiGetDeviceInterfaceDetail(h, &d, 0, 0, &sz, 0);
        if (!sz) continue;

        std::vector<BYTE> b(sz);
        auto p = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(b.data());
        p->cbSize = sizeof(*p);

        if (SetupDiGetDeviceInterfaceDetail(h, &d, p, sz, 0, 0)) {
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
    if (IsConnected()) return true;

    // Grab device path
    std::wstring path; 
    if (!FindPowerMateDevicePath(path)) {
        std::cerr << "[Debug] PowerMate device not found\n";
        return false;
    }

    // Close old handle if valid
    HANDLE oldHandle = hDevice.load();
    if (oldHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(oldHandle);
        hDevice.store(INVALID_HANDLE_VALUE); // Reset handle
    } 

    // Open the device
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        std::cerr << "[Debug] Failed to open Powermate\n";
        return false;
    }

    hDevice.store(h);
    connected.store(true);
    StartReading();
    std::cerr << "[Debug] Powermate device connected\n";
    return true;
}

// Start reading inputs
void PowermateManager::StartReading() {
    std::lock_guard<std::mutex> lock(deviceMutex);

    if (running) return;
    if (!IsConnected() || hDevice.load() == INVALID_HANDLE_VALUE) return;

    running = true;
    inputThread = std::thread(&PowermateManager::InputLoop);
}

// Handle device change (arrival/removal)
void PowermateManager::HandleDeviceChange(WPARAM wParam) {
    std::wstring path;

    if (wParam == DBT_DEVICEARRIVAL && FindAndOpenDevice()) return;

    else if (wParam == DBT_DEVICEREMOVECOMPLETE) {
        if (FindPowerMateDevicePath(path)) {
            std::wcerr << L"[Debug] Powermate is still connected\n";
        } else {
            std::wcerr << L"[Debug] Powermate is no longer connected\n";
            connected.store(false);
            Stop();
        }
    } 
    else if (wParam == PBT_APMSUSPEND) {
        std::wcout << L"[Debug] Suspending, stopping device\n";
        Stop();
    } 
    else if (wParam == PBT_APMRESUMESUSPEND) {
        if (FindAndOpenDevice()) {
        } else {
            connected.store(false);
            std::wcerr << L"[Debug] Device could not found after system resume\n";
        }
    }
}


// Intercepting device inputs
void PowermateManager::InputLoop() {
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        std::cerr << "[Error] Failed to create overlapped event\n";
        return;
    }

    unsigned char buffer[8] = {};
    DWORD bytesRead = 0;
    bool buttonDown = false;
    DWORD pressStartTime = 0;
    bool isLongPress = false;

    while (running && connected) {
        ResetEvent(overlapped.hEvent);

        if (!ReadFile(hDevice, buffer, sizeof(buffer), nullptr, &overlapped)) {
            DWORD err = GetLastError();
            if (err == ERROR_DEVICE_NOT_CONNECTED) {
                std::cerr << "[Error] Device no longer connected\n";
                connected = false;
                break;
            }

            if (err == ERROR_IO_PENDING) {
                DWORD wait = WaitForSingleObject(overlapped.hEvent, 50);
                if (wait == WAIT_OBJECT_0) {
                    if (!GetOverlappedResult(hDevice, &overlapped, &bytesRead, FALSE)) break;
                } else if (wait == WAIT_TIMEOUT) {
                    // Handle Long press logic
                    if (buttonDown && !isLongPress && GetTickCount() - pressStartTime >= LONG_PRESS_THRESHOLD) {
                        std::cout << "LONG PRESS DETECTED\n";
                        HandleInput(PowermateInputType::LONG_PRESS);
                        MessageBeep(MB_ICONASTERISK);
                        isLongPress = true;
                    }
                    continue;
                } else break;
            } else {
                std::cerr << "[Error] ReadFile failed: " << err << "\n";
                break;
            }
        }

        if (bytesRead < 3) continue;

        // Handle rotation
        int8_t rotation = static_cast<int8_t>(buffer[2]);
        if (rotation != 0) {
            std::cout << (rotation < 0 ? "ROTATE RIGHT" : "ROTATE LEFT") << " (" << static_cast<int>(rotation) << ")\n";
            HandleInput(rotation < 0 ? PowermateInputType::ROTATE_RIGHT : PowermateInputType::ROTATE_LEFT);
        }

        // Handle Button press/release
        bool isPressed = buffer[1] == 1;
        if (isPressed != buttonDown) {
            buttonDown = isPressed;
            if (buttonDown) {
                pressStartTime = GetTickCount();
                isLongPress = false;
                std::cout << "BUTTON PRESSED\n";
            } else {
                std::cout << "BUTTON RELEASED\n";
                if (!isLongPress) HandleInput(PowermateInputType::BUTTON_RELEASE);
            }
        }
    }

    CloseHandle(overlapped.hEvent);
}

// Forward input to TriggerAction
void PowermateManager::HandleInput(PowermateInputType inputType) {
    TriggerAction::HandleAction(inputType);
}

// Stop reading inputs
void PowermateManager::Stop() {
    std::lock_guard<std::mutex> lock(deviceMutex);
    if (!running) return;
    running = false;
    connected.store(false);
    if (hDevice.load() != INVALID_HANDLE_VALUE) {
        CancelIo(hDevice.load());
    }
    if (inputThread.joinable()) {
        inputThread.join();
    }
    CloseDevice();
}

// Close the device handle
void PowermateManager::CloseDevice() {
    HANDLE device = hDevice.load();
    if (device != INVALID_HANDLE_VALUE) {
        CloseHandle(device);
        hDevice.store(INVALID_HANDLE_VALUE);
    }
}