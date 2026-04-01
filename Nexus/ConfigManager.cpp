#include "ConfigManager.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "shell32.lib")

static std::wstring GetConfigPath()
{
    WCHAR path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
    {
        std::wstring dir = std::wstring(path) + L"\\Nexus";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\config.ini";
    }
    return L"";
}

static void WriteDefaultConfig(const std::wstring& path)
{
    WritePrivateProfileStringW(L"General", L"Hotkey", L"Ctrl+Alt+K", path.c_str());
    WritePrivateProfileStringW(L"General", L"TimeoutMs", L"1000", path.c_str());
    WritePrivateProfileStringW(L"Overlay", L"FontSize", L"12", path.c_str());
    WritePrivateProfileStringW(L"Overlay", L"BgAlpha", L"120", path.c_str());
    WritePrivateProfileStringW(L"Overlay", L"CornerRadius", L"4", path.c_str());
    WritePrivateProfileStringW(L"Overlay", L"TagPosition", L"bottom", path.c_str());
    WritePrivateProfileStringW(L"Mouse", L"Modes", L"left,right,middle,double_click", path.c_str());
}

static unsigned int ParseHotkeyModifiers(const std::wstring& token)
{
    std::wstring lower = token;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower == L"ctrl" || lower == L"control") return MOD_CONTROL;
    if (lower == L"alt") return MOD_ALT;
    if (lower == L"shift") return MOD_SHIFT;
    if (lower == L"win" || lower == L"windows") return MOD_WIN;
    return 0;
}

static bool ParseHotkey(const std::wstring& hotkeyStr, unsigned int& outMods, unsigned int& outVk)
{
    outMods = 0;
    outVk = 0;

    std::wstringstream ss(hotkeyStr);
    std::wstring token;
    std::vector<std::wstring> parts;

    while (std::getline(ss, token, L'+'))
    {
        // trim
        token.erase(token.begin(), std::find_if(token.begin(), token.end(), [](wchar_t ch) { return !iswspace(ch); }));
        token.erase(std::find_if(token.rbegin(), token.rend(), [](wchar_t ch) { return !iswspace(ch); }).base(), token.end());
        if (!token.empty())
            parts.push_back(token);
    }

    if (parts.empty()) return false;

    for (size_t i = 0; i + 1 < parts.size(); ++i)
    {
        outMods |= ParseHotkeyModifiers(parts[i]);
    }

    std::wstring key = parts.back();
    if (key.length() == 1)
    {
        outVk = static_cast<unsigned int>(towupper(key[0]));
    }
    else
    {
        std::transform(key.begin(), key.end(), key.begin(), ::towlower);
        if (key == L"f1") outVk = VK_F1;
        else if (key == L"f2") outVk = VK_F2;
        else if (key == L"f3") outVk = VK_F3;
        else if (key == L"f4") outVk = VK_F4;
        else if (key == L"f5") outVk = VK_F5;
        else if (key == L"f6") outVk = VK_F6;
        else if (key == L"f7") outVk = VK_F7;
        else if (key == L"f8") outVk = VK_F8;
        else if (key == L"f9") outVk = VK_F9;
        else if (key == L"f10") outVk = VK_F10;
        else if (key == L"f11") outVk = VK_F11;
        else if (key == L"f12") outVk = VK_F12;
        else return false;
    }

    return true;
}

static MouseMode ParseMouseMode(const std::wstring& token)
{
    std::wstring lower = token;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (lower == L"right") return MouseMode::Right;
    if (lower == L"middle") return MouseMode::Middle;
    if (lower == L"double_click" || lower == L"double") return MouseMode::LeftDoubleClick;
    return MouseMode::Left;
}

static std::vector<MouseMode> ParseMouseModes(const std::wstring& str)
{
    std::vector<MouseMode> modes;
    std::wstringstream ss(str);
    std::wstring token;
    while (std::getline(ss, token, L','))
    {
        token.erase(token.begin(), std::find_if(token.begin(), token.end(), [](wchar_t ch) { return !iswspace(ch); }));
        token.erase(std::find_if(token.rbegin(), token.rend(), [](wchar_t ch) { return !iswspace(ch); }).base(), token.end());
        if (!token.empty())
            modes.push_back(ParseMouseMode(token));
    }
    if (modes.empty())
    {
        modes = { MouseMode::Left, MouseMode::Right, MouseMode::Middle, MouseMode::LeftDoubleClick };
    }
    return modes;
}

NexusConfig LoadConfig()
{
    NexusConfig cfg;
    std::wstring path = GetConfigPath();
    if (path.empty()) return cfg;

    // 如果文件不存在，创建默认配置
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        WriteDefaultConfig(path);
    }

    WCHAR buf[256] = {};

    GetPrivateProfileStringW(L"General", L"Hotkey", L"Ctrl+Alt+K", buf, 256, path.c_str());
    cfg.hotkey = buf;
    if (!ParseHotkey(cfg.hotkey, cfg.hotkeyModifiers, cfg.hotkeyVk))
    {
        cfg.hotkey = L"Ctrl+Alt+K";
        cfg.hotkeyModifiers = MOD_CONTROL | MOD_ALT;
        cfg.hotkeyVk = 'K';
    }

    cfg.timeoutMs = GetPrivateProfileIntW(L"General", L"TimeoutMs", 1000, path.c_str());
    cfg.overlayFontSize = GetPrivateProfileIntW(L"Overlay", L"FontSize", 12, path.c_str());
    cfg.overlayBgAlpha = GetPrivateProfileIntW(L"Overlay", L"BgAlpha", 120, path.c_str());
    cfg.overlayCornerRadius = GetPrivateProfileIntW(L"Overlay", L"CornerRadius", 4, path.c_str());

    GetPrivateProfileStringW(L"Overlay", L"TagPosition", L"bottom", buf, 256, path.c_str());
    cfg.tagPosition = buf;

    GetPrivateProfileStringW(L"Mouse", L"Modes", L"left,right,middle,double_click", buf, 256, path.c_str());
    cfg.mouseModes = ParseMouseModes(buf);

    return cfg;
}
