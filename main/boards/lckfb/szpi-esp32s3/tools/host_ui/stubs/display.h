#pragma once
#include <string>
class Display {
public:
    int GetTopBarBottom() const { return 24; }
    bool IsSetupUICalled() const { return true; }
    void ShowNotification(const char*, int) {}
};
class DisplayLockGuard {
public:
    DisplayLockGuard(Display*) {}
    explicit operator bool() const { return true; }
};
