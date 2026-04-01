#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct ClickableElement {
    RECT rect;
    std::wstring label;
};

std::vector<ClickableElement> CollectElements();
