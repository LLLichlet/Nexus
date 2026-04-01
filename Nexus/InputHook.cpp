#include "InputHook.h"

static HWND g_hNotifyWnd = nullptr;
static HHOOK g_hHook = nullptr;
static char g_firstKey = 0;
static DWORD g_keyTimeout = 1000;

static void PostTrigger(char first, char second)
{
    if (g_hNotifyWnd)
    {
        PostMessage(g_hNotifyWnd, WM_USER_TRIGGER_CLICK, static_cast<WPARAM>(first), static_cast<LPARAM>(second));
        PostMessage(g_hNotifyWnd, WM_USER_FIRST_KEY, 0, 0);
    }
    g_firstKey = 0;
}

static void PostExit()
{
    if (g_hNotifyWnd)
    {
        PostMessage(g_hNotifyWnd, WM_USER_EXIT_MODE, 0, 0);
        PostMessage(g_hNotifyWnd, WM_USER_FIRST_KEY, 0, 0);
    }
    g_firstKey = 0;
}

static void PostSwitchMode()
{
    if (g_hNotifyWnd)
        PostMessage(g_hNotifyWnd, WM_USER_SWITCH_MODE, 0, 0);
}

static void PostFirstKey(char key)
{
    if (g_hNotifyWnd)
        PostMessage(g_hNotifyWnd, WM_USER_FIRST_KEY, static_cast<WPARAM>(key), 0);
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
    {
        KBDLLHOOKSTRUCT* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD now = GetTickCount();

        static DWORD lastVk = 0;
        static DWORD lastTick = 0;
        if (pKbd->vkCode == lastVk && (now - lastTick < 80))
        {
            return 1;
        }
        lastVk = pKbd->vkCode;
        lastTick = now;

        if (pKbd->vkCode == VK_ESCAPE)
        {
            PostExit();
            return 1;
        }

        if (pKbd->vkCode == VK_TAB)
        {
            PostSwitchMode();
            return 1;
        }

        if (pKbd->vkCode >= 'A' && pKbd->vkCode <= 'Z')
        {
            if (g_firstKey == 0)
            {
                g_firstKey = static_cast<char>(pKbd->vkCode);
                PostFirstKey(g_firstKey);
            }
            else
            {
                PostTrigger(g_firstKey, static_cast<char>(pKbd->vkCode));
                return 1;
            }
            return 1;
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

bool InstallInputHook(HWND hWndNotify)
{
    if (g_hHook) return true;
    g_hNotifyWnd = hWndNotify;
    g_firstKey = 0;
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
    return g_hHook != nullptr;
}

bool UninstallInputHook()
{
    if (g_hHook)
    {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = nullptr;
    }
    g_hNotifyWnd = nullptr;
    g_firstKey = 0;
    return true;
}

void ResetInputHookFirstKey()
{
    g_firstKey = 0;
}

void SetInputTimeout(int ms)
{
    if (ms < 100) ms = 100;
    if (ms > 5000) ms = 5000;
    g_keyTimeout = static_cast<DWORD>(ms);
}
