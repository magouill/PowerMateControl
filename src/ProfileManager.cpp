#include "ProfileManager.h"
#include <iostream>

// Initialize static variable
size_t ProfileManager::currentProfileIndex = 0;

// Static method: Return a reference to a static vector of profile names
const std::vector<std::wstring>& ProfileManager::GetProfileList() {
    static const std::vector<std::wstring> profiles = {L"Scroll", L"Volume"};
    return profiles;
}

// Static method: Return the current profile index
size_t ProfileManager::GetCurrentProfileIndex() {
    return currentProfileIndex;
}

// Static method: Return the name of the current profile
std::wstring ProfileManager::GetCurrentProfileName() {
    const auto& profiles = GetProfileList();
    size_t idx = GetCurrentProfileIndex();
    return (idx < profiles.size()) ? profiles[idx] : L"(Invalid Profile)";
}

// Static method: Set the current profile by index
void ProfileManager::SetCurrentProfile(int index) {
    if (index >= 0 && index < GetProfileList().size()) {
        currentProfileIndex = index;
        std::wcout << L"[Debug] Current Profile set to: " << GetProfileList()[index] << std::endl;
    } else {
        std::wcout << L"[Error] Invalid profile index" << std::endl;
    }
}