#pragma once

#include <windows.h>
#include <vector>
#include "ElementCollector.h"

enum class MouseMode {
    Left,
    Right,
    Middle,
    LeftDoubleClick
};

void ShowOverlay(const std::vector<ClickableElement>& elements, MouseMode mode);
void HideOverlay();
void SetOverlayMouseMode(MouseMode mode);
void SetOverlayFirstKey(char key);
void SetOverlayStyle(int fontSize, int bgAlpha, int cornerRadius);
void SetOverlayTagPosition(const std::wstring& pos);
