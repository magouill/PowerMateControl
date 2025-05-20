#pragma once

#include "TriggerAction.h"
#include <Windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>

class PowermateManager {
public:
    // Find Powermate device path in the system
    static bool FindPowerMateDevicePath(std::wstring& outPath);

    // Check if device is currently connected and open
    static bool IsConnected();

    // Try to find and open the Powermate device
    static bool FindAndOpenDevice();

    // Start reading input events in background thread
    static void StartReading();

    // Stop reading input and close device
    static void Stop();

    // Handle device change events (plug/unplug/suspend/resume)
    static void HandleDeviceChange(WPARAM wParam);

    // Forward input events to the TriggerAction handler
    static void HandleInput(PowermateInputType inputType);

private:
    // The main loop reading input reports from device
    static void InputLoop();

    // Close the device handle safely
    static void CloseDevice();

    // Atomic flags and device handle
    static std::atomic<bool> running;
    static std::atomic<bool> connected;
    static std::atomic<HANDLE> hDevice;

    // Input thread and synchronization mutex
    static std::thread inputThread;
    static std::mutex deviceMutex;
};
