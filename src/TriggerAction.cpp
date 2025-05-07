#include "TriggerAction.h"
#include "ProfileManager.h"
#include <iostream>

namespace {

// Helper to simulate a key press
void SendKey(WORD key, bool extended = false) {
    INPUT input[2] = {};

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = key;
    if (extended) input[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = key;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP | (extended ? KEYEVENTF_EXTENDEDKEY : 0);

    SendInput(2, input, sizeof(INPUT));
}

// Helper to simulate mouse double click
void SendMouseDoubleClick() {
    INPUT input[4] = {};

    for (int i = 0; i < 2; ++i) {
        input[i * 2].type = INPUT_MOUSE;
        input[i * 2].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

        input[i * 2 + 1].type = INPUT_MOUSE;
        input[i * 2 + 1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    }

    SendInput(4, input, sizeof(INPUT));
}

// Helpers for volume control
void ChangeVolume(bool increase) {
    SendKey(increase ? VK_VOLUME_UP : VK_VOLUME_DOWN);
}

void ToggleMute() {
    SendKey(VK_VOLUME_MUTE);
}

// Helpers for scroll
void ScrollMouse(int amount) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = amount;
    SendInput(1, &input, sizeof(INPUT));
}

}  // namespace

// Handle different actions based on profile and input type
void TriggerAction::HandleAction(PowermateInputType inputType) {
    size_t profileIndex = ProfileManager::GetCurrentProfileIndex();

    if (profileIndex == 0) { // Scroll profile
        switch (inputType) {
            case PowermateInputType::ROTATE_LEFT:
                ScrollMouse(-WHEEL_DELTA);  // Scroll left
                break;
            case PowermateInputType::ROTATE_RIGHT:
                ScrollMouse(+WHEEL_DELTA);  // Scroll right
                break;
            case PowermateInputType::BUTTON_RELEASE:
                SendMouseDoubleClick();  // Double click on button release
                break;
            case PowermateInputType::LONG_PRESS:
                // Switch to Volume profile
                ProfileManager::SetCurrentProfile(1);
                break;
        }
    }
    else if (profileIndex == 1) { // Volume profile
        switch (inputType) {
            case PowermateInputType::ROTATE_LEFT:
                ChangeVolume(true);  // Increase volume
                break;
            case PowermateInputType::ROTATE_RIGHT:
                ChangeVolume(false);  // Decrease volume
                break;
            case PowermateInputType::BUTTON_RELEASE:
                ToggleMute();  // Mute/unmute
                break;
            case PowermateInputType::LONG_PRESS:
                // Switch to Scroll profile
                ProfileManager::SetCurrentProfile(0);
                break;
        }
    }
}
