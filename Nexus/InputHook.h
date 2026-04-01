#pragma once

#include <windows.h>

#define WM_USER_EXIT_MODE     (WM_USER + 10)
#define WM_USER_SWITCH_MODE   (WM_USER + 11)
#define WM_USER_TRIGGER_CLICK (WM_USER + 12)
#define WM_USER_FIRST_KEY     (WM_USER + 13)

bool InstallInputHook(HWND hWndNotify);
bool UninstallInputHook();
void ResetInputHookFirstKey();
void SetInputTimeout(int ms);
