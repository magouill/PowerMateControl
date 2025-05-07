#pragma once
#include <Windows.h>

// Enum to represent different types of Powermate input events
enum PowermateInputType {
    ROTATE_LEFT,    // Rotate left input
    ROTATE_RIGHT,   // Rotate right input
    BUTTON_RELEASE, // Button release event
    LONG_PRESS,     // Long press event
};

class TriggerAction {
public:
    // Handles actions based on the input type (like rotation or button press)
    static void HandleAction(PowermateInputType inputType);
};