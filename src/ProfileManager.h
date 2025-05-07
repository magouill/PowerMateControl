#pragma once
#include <string>
#include <vector>

class ProfileManager {
public:
    // Static methods to handle profile management
    static void SetCurrentProfile(int index);
    static const std::vector<std::wstring>& GetProfileList();
    static size_t GetCurrentProfileIndex();
    static std::wstring GetCurrentProfileName();

private:
    // Static variable to store current profile index
    static size_t currentProfileIndex;
};