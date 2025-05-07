#pragma once

#include "TriggerAction.h"
#include <Windows.h>
#include <atomic>
#include <thread>
#include <mutex>

class PowermateManager {
public:
    // Device initialization and connection
    static bool FindPowerMateDevicePath(std::wstring& outPath);
    static bool IsConnected();
    static bool FindAndOpenDevice();
    
    // Input management
    static void StartReading();
    static void Stop();

    // Handle device connection and disconnection
    static void HandleDeviceChange(WPARAM wParam);
    
    // Forward input to TriggerAction
    static void HandleInput(PowermateInputType inputType);

private:
    // Internal device handling and input processing
    static void InputLoop();
    static void CloseDevice();

    // Long press detection
    static DWORD pressStartTime;
    static bool isLongPress;
    static const DWORD LONG_PRESS_THRESHOLD;

    // Atomic states and thread
    static std::atomic<bool> running;
    static std::atomic<bool> connected;
    static std::atomic<HANDLE> hDevice;
    static std::thread inputThread;
    static std::mutex deviceMutex;
};