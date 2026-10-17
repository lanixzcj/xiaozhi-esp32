#pragma once
#include <string>
class Display {
public:
    bool IsSetupUICalled() const { return true; }
    void ShowNotification(const char*, int) {}
};
class DisplayLockGuard {
public:
    DisplayLockGuard(Display*) {}
    explicit operator bool() const { return true; }
};
