// Nexus.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "Nexus.h"
#include "ElementCollector.h"
#include "OverlayWindow.h"
#include "InputHook.h"
#include "ConfigManager.h"
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <combaseapi.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "shell32.lib")

#define MAX_LOADSTRING 100
#define WM_TRAYICON (WM_USER + 1)
#define HOTKEY_ID   1

// 全局变量
HINSTANCE hInst;
WCHAR szWindowClass[MAX_LOADSTRING];
ULONG_PTR g_gdiplusToken = 0;
NOTIFYICONDATAW g_nid = {};

static NexusConfig g_config;
static bool g_isActive = false;
static MouseMode g_currentMode = MouseMode::Left;
static std::vector<ClickableElement> g_activeElements;

// 前向声明
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void                AddTrayIcon(HWND hWnd);
void                RemoveTrayIcon();
void                ShowTrayMenu(HWND hWnd);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_config = LoadConfig();
    SetInputTimeout(g_config.timeoutMs);
    SetOverlayStyle(g_config.overlayFontSize, g_config.overlayBgAlpha, g_config.overlayCornerRadius);
    SetOverlayTagPosition(g_config.tagPosition);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, L"COM initialization failed.", L"Nexus", MB_OK | MB_ICONERROR);
        return 1;
    }

    Gdiplus::GdiplusStartupInput gdiplusInput;
    gdiplusInput.GdiplusVersion = 1;
    gdiplusInput.DebugEventCallback = nullptr;
    gdiplusInput.SuppressBackgroundThread = FALSE;
    gdiplusInput.SuppressExternalCodecs = FALSE;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr);

    LoadStringW(hInstance, IDC_NEXUS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, SW_HIDE))
    {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        CoUninitialize();
        return FALSE;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    CoUninitialize();

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = WndProc;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NEXUS));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, L"Nexus", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    if (!RegisterHotKey(hWnd, HOTKEY_ID, g_config.hotkeyModifiers, g_config.hotkeyVk))
    {
        MessageBoxW(nullptr, L"Failed to register global hotkey. It may be in use by another application.", L"Nexus", MB_OK | MB_ICONWARNING);
    }

    AddTrayIcon(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
        {
            ShowTrayMenu(hWnd);
        }
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_HOTKEY:
        if (wParam == HOTKEY_ID)
        {
            if (!g_isActive)
            {
                g_activeElements = CollectElements();
                if (!g_activeElements.empty())
                {
                    g_currentMode = g_config.mouseModes.empty() ? MouseMode::Left : g_config.mouseModes.front();
                    ShowOverlay(g_activeElements, g_currentMode);
                    if (InstallInputHook(hWnd))
                    {
                        g_isActive = true;
                    }
                    else
                    {
                        HideOverlay();
                        g_activeElements.clear();
                    }
                }
            }
        }
        break;

    case WM_USER_EXIT_MODE:
        if (g_isActive)
        {
            KillTimer(hWnd, 1);
            HideOverlay();
            UninstallInputHook();
            g_isActive = false;
            g_activeElements.clear();
        }
        break;

    case WM_USER_SWITCH_MODE:
        if (g_isActive)
        {
            {
                auto it = std::find(g_config.mouseModes.begin(), g_config.mouseModes.end(), g_currentMode);
                if (it != g_config.mouseModes.end())
                {
                    ++it;
                    if (it == g_config.mouseModes.end())
                        it = g_config.mouseModes.begin();
                    g_currentMode = *it;
                }
                else
                {
                    g_currentMode = g_config.mouseModes.empty() ? MouseMode::Left : g_config.mouseModes.front();
                }
                SetOverlayMouseMode(g_currentMode);
            }
        }
        break;

    case WM_USER_FIRST_KEY:
        if (g_isActive)
        {
            SetOverlayFirstKey(static_cast<char>(wParam));
            if (wParam != 0)
            {
                SetTimer(hWnd, 1, 1000, nullptr);
            }
            else
            {
                KillTimer(hWnd, 1);
            }
        }
        break;

    case WM_TIMER:
        if (wParam == 1 && g_isActive)
        {
            SetOverlayFirstKey(0);
            ResetInputHookFirstKey();
            KillTimer(hWnd, 1);
        }
        break;

    case WM_USER_TRIGGER_CLICK:
        if (g_isActive)
        {
            char first = static_cast<char>(wParam);
            char second = static_cast<char>(lParam);
            std::wstring label = std::wstring{ static_cast<wchar_t>(first), static_cast<wchar_t>(second) };

            const ClickableElement* target = nullptr;
            for (const auto& e : g_activeElements)
            {
                if (e.label == label)
                {
                    target = &e;
                    break;
                }
            }

            if (target)
            {
                int x = (target->rect.left + target->rect.right) / 2;
                int y = (target->rect.top + target->rect.bottom) / 2;
                SetCursorPos(x, y);

                bool isDoubleClick = false;
                DWORD downFlag = 0, upFlag = 0;
                switch (g_currentMode)
                {
                case MouseMode::LeftDoubleClick:
                    isDoubleClick = true;
                    downFlag = MOUSEEVENTF_LEFTDOWN;
                    upFlag = MOUSEEVENTF_LEFTUP;
                    break;
                case MouseMode::Left:
                    downFlag = MOUSEEVENTF_LEFTDOWN;
                    upFlag = MOUSEEVENTF_LEFTUP;
                    break;
                case MouseMode::Right:
                    downFlag = MOUSEEVENTF_RIGHTDOWN;
                    upFlag = MOUSEEVENTF_RIGHTUP;
                    break;
                case MouseMode::Middle:
                    downFlag = MOUSEEVENTF_MIDDLEDOWN;
                    upFlag = MOUSEEVENTF_MIDDLEUP;
                    break;
                }

                if (isDoubleClick)
                {
                    INPUT inputs[4] = {};
                    for (int i = 0; i < 2; ++i)
                    {
                        inputs[i * 2].type = INPUT_MOUSE;
                        inputs[i * 2].mi.dwFlags = downFlag;
                        inputs[i * 2 + 1].type = INPUT_MOUSE;
                        inputs[i * 2 + 1].mi.dwFlags = upFlag;
                    }
                    SendInput(4, inputs, sizeof(INPUT));
                }
                else
                {
                    INPUT inputs[2] = {};
                    inputs[0].type = INPUT_MOUSE;
                    inputs[0].mi.dwFlags = downFlag;
                    inputs[1].type = INPUT_MOUSE;
                    inputs[1].mi.dwFlags = upFlag;
                    SendInput(2, inputs, sizeof(INPUT));
                }
            }

            KillTimer(hWnd, 1);
            HideOverlay();
            UninstallInputHook();
            g_isActive = false;
            g_activeElements.clear();
        }
        break;

    case WM_DESTROY:
        if (g_isActive)
        {
            HideOverlay();
            UninstallInputHook();
            g_isActive = false;
            g_activeElements.clear();
        }
        RemoveTrayIcon();
        UnregisterHotKey(hWnd, HOTKEY_ID);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void AddTrayIcon(HWND hWnd)
{
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_NEXUS));
    wcscpy_s(g_nid.szTip, L"Nexus - Keyboard Mouse");

    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (hMenu)
    {
        AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"About");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);
    }
}
