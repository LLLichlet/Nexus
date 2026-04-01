#include "ElementCollector.h"
#include <uiautomation.h>
#include <algorithm>
#include <cstdlib>

static bool IsRectSimilar(const RECT& a, const RECT& b)
{
    int centerAX = (a.left + a.right) / 2;
    int centerAY = (a.top + a.bottom) / 2;
    int centerBX = (b.left + b.right) / 2;
    int centerBY = (b.top + b.bottom) / 2;

    int dx = centerAX - centerBX;
    int dy = centerAY - centerBY;
    if (dx * dx + dy * dy < 100) // 中心点距离 < 10px
        return true;

    int interLeft = max(a.left, b.left);
    int interTop = max(a.top, b.top);
    int interRight = min(a.right, b.right);
    int interBottom = min(a.bottom, b.bottom);

    if (interRight <= interLeft || interBottom <= interTop)
        return false;

    int interArea = (interRight - interLeft) * (interBottom - interTop);
    int areaA = (a.right - a.left) * (a.bottom - a.top);
    int areaB = (b.right - b.left) * (b.bottom - b.top);
    int minArea = min(areaA, areaB);

    return minArea > 0 && (interArea * 2 > minArea);
}

static void DeduplicateAndFilter(std::vector<ClickableElement>& elements)
{
    std::vector<ClickableElement> result;

    int vLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vRight = vLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vBottom = vTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    for (auto& elem : elements)
    {
        RECT& r = elem.rect;
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        if (w < 10 || h < 10) continue;
        if (r.right < vLeft || r.bottom < vTop || r.left > vRight || r.top > vBottom) continue;

        bool dup = false;
        for (const auto& ex : result)
        {
            if (IsRectSimilar(r, ex.rect))
            {
                dup = true;
                break;
            }
        }
        if (!dup)
            result.push_back(elem);
    }

    elements = std::move(result);
}

static void SortAndLabel(std::vector<ClickableElement>& elements)
{
    const int rowThreshold = 40;
    std::sort(elements.begin(), elements.end(),
        [rowThreshold](const ClickableElement& a, const ClickableElement& b)
        {
            if (std::abs(a.rect.top - b.rect.top) < rowThreshold)
                return a.rect.left < b.rect.left;
            return a.rect.top < b.rect.top;
        });

    size_t idx = 0;
    // 使用 wchar_t 以避免从 char 到 wchar_t 的窄化转换
    for (wchar_t c1 = L'A'; c1 <= L'Z' && idx < elements.size(); ++c1)
    {
        for (wchar_t c2 = L'A'; c2 <= L'Z' && idx < elements.size(); ++c2)
        {
            // 安全地构造宽字符串标签
            elements[idx].label = std::wstring(1, c1) + std::wstring(1, c2);
            ++idx;
        }
    }
    elements.resize(idx);
}

static void CollectUIAElements(std::vector<ClickableElement>& out)
{
    IUIAutomation* pAuto = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr,
        CLSCTX_INPROC_SERVER, IID_IUIAutomation, reinterpret_cast<void**>(&pAuto));
    if (FAILED(hr) || !pAuto) return;

    HWND fgWnd = GetForegroundWindow();
    if (!fgWnd)
    {
        pAuto->Release();
        return;
    }

    IUIAutomationElement* pRoot = nullptr;
    hr = pAuto->ElementFromHandle(fgWnd, &pRoot);
    if (FAILED(hr) || !pRoot) { pAuto->Release(); return; }

    VARIANT varFalse = {};
    varFalse.vt = VT_BOOL;
    varFalse.boolVal = VARIANT_FALSE;

    IUIAutomationCondition* pCond = nullptr;
    hr = pAuto->CreatePropertyCondition(UIA_IsOffscreenPropertyId, varFalse, &pCond);
    if (FAILED(hr) || !pCond) { pRoot->Release(); pAuto->Release(); return; }

    IUIAutomationElementArray* pArr = nullptr;
    hr = pRoot->FindAll(TreeScope_Descendants, pCond, &pArr);
    if (SUCCEEDED(hr) && pArr)
    {
        int len = 0;
        pArr->get_Length(&len);
        for (int i = 0; i < len; ++i)
        {
            IUIAutomationElement* pElem = nullptr;
            pArr->GetElement(i, &pElem);
            if (!pElem) continue;

            RECT rc = {};
            hr = pElem->get_CurrentBoundingRectangle(&rc);
            if (SUCCEEDED(hr))
            {
                bool ok = false;
                VARIANT var = {};
                if (SUCCEEDED(pElem->GetCurrentPropertyValue(UIA_IsInvokePatternAvailablePropertyId, &var)) &&
                    var.vt == VT_BOOL && var.boolVal == VARIANT_TRUE)
                {
                    ok = true;
                }
                VariantClear(&var);

                if (!ok)
                {
                    if (SUCCEEDED(pElem->GetCurrentPropertyValue(UIA_IsTogglePatternAvailablePropertyId, &var)) &&
                        var.vt == VT_BOOL && var.boolVal == VARIANT_TRUE)
                    {
                        ok = true;
                    }
                    VariantClear(&var);
                }

                if (!ok)
                {
                    CONTROLTYPEID controlType = UIA_CustomControlTypeId;
                    if (SUCCEEDED(pElem->get_CurrentControlType(&controlType)))
                    {
                        if (controlType == UIA_ButtonControlTypeId ||
                            controlType == UIA_MenuItemControlTypeId ||
                            controlType == UIA_HyperlinkControlTypeId ||
                            controlType == UIA_TabItemControlTypeId ||
                            controlType == UIA_ListItemControlTypeId ||
                            controlType == UIA_TreeItemControlTypeId ||
                            controlType == UIA_SplitButtonControlTypeId ||
                            controlType == UIA_ComboBoxControlTypeId ||
                            controlType == UIA_CheckBoxControlTypeId ||
                            controlType == UIA_RadioButtonControlTypeId ||
                            controlType == UIA_EditControlTypeId ||
                            controlType == UIA_DocumentControlTypeId ||
                            controlType == UIA_GroupControlTypeId)
                        {
                            ok = true;
                        }
                    }
                }

                if (ok)
                    out.push_back({ rc, L"" });
            }
            pElem->Release();
        }
        pArr->Release();
    }

    pCond->Release();
    pRoot->Release();
    pAuto->Release();
}

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd)) return TRUE;

    RECT rc = {};
    if (!GetWindowRect(hwnd, &rc)) return TRUE;

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 10 || h < 10) return TRUE;

    WCHAR className[256] = {};
    GetClassNameW(hwnd, className, 256);
    LONG style = GetWindowLong(hwnd, GWL_STYLE);

    bool interactive = false;
    if (wcscmp(className, L"Button") == 0 ||
        wcscmp(className, L"SysLink") == 0 ||
        wcscmp(className, L"SysListView32") == 0 ||
        wcscmp(className, L"SysTreeView32") == 0 ||
        wcscmp(className, L"Edit") == 0 ||
        wcscmp(className, L"RichEdit20A") == 0 ||
        wcscmp(className, L"RichEdit20W") == 0 ||
        wcscmp(className, L"RICHEDIT50W") == 0)
    {
        interactive = true;
    }
    if (style & WS_TABSTOP) interactive = true;
    if ((style & 0x0F) == BS_PUSHBUTTON) interactive = true;

    if (interactive)
    {
        auto* pVec = reinterpret_cast<std::vector<ClickableElement>*>(lParam);
        pVec->push_back({ rc, L"" });
    }

    return TRUE;
}

static void CollectWin32Elements(std::vector<ClickableElement>& out)
{
    HWND fgWnd = GetForegroundWindow();
    if (!fgWnd || !IsWindowVisible(fgWnd)) return;
    EnumChildWindows(fgWnd, EnumChildProc, reinterpret_cast<LPARAM>(&out));
}

std::vector<ClickableElement> CollectElements()
{
    static HWND s_lastHwnd = nullptr;
    static RECT s_lastRect = {};
    static DWORD s_lastTick = 0;
    static std::vector<ClickableElement> s_cachedElements;

    constexpr DWORD CACHE_TTL_MS = 5000;

    HWND fgWnd = GetForegroundWindow();
    if (!fgWnd)
    {
        s_lastHwnd = nullptr;
        s_cachedElements.clear();
        return {};
    }

    RECT currentRect = {};
    GetWindowRect(fgWnd, &currentRect);

    DWORD now = GetTickCount();
    bool cacheValid = (fgWnd == s_lastHwnd)
        && (now - s_lastTick < CACHE_TTL_MS)
        && (EqualRect(&currentRect, &s_lastRect));

    if (cacheValid)
    {
        return s_cachedElements;
    }

    std::vector<ClickableElement> elements;
    CollectUIAElements(elements);
    CollectWin32Elements(elements);
    DeduplicateAndFilter(elements);
    SortAndLabel(elements);

    s_lastHwnd = fgWnd;
    s_lastRect = currentRect;
    s_lastTick = now;
    s_cachedElements = elements;

    return elements;
}
