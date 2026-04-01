#pragma once

#include <string>
#include <vector>
#include "OverlayWindow.h"

struct NexusConfig
{
    std::wstring hotkey = L"Ctrl+Alt+K";
    unsigned int hotkeyModifiers = MOD_CONTROL | MOD_ALT;
    unsigned int hotkeyVk = 'K';

    int timeoutMs = 1000;
    int overlayFontSize = 12;
    int overlayBgAlpha = 120;
    int overlayCornerRadius = 4;
    std::wstring tagPosition = L"bottom";

    std::vector<MouseMode> mouseModes = {
        MouseMode::Left,
        MouseMode::Right,
        MouseMode::Middle,
        MouseMode::LeftDoubleClick
    };
};

NexusConfig LoadConfig();
