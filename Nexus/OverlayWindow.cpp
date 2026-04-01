#include "OverlayWindow.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

extern HINSTANCE hInst;

static HWND g_hOverlayWnd = nullptr;
static std::vector<ClickableElement> g_elements;
static MouseMode g_currentMode = MouseMode::Left;
static char g_firstKey = 0;
static int g_fontSize = 12;
static int g_bgAlpha = 120;
static int g_cornerRadius = 4;
static std::wstring g_tagPosition = L"bottom";

namespace {

Gdiplus::Color HslToColor(double h, double s, double l)
{
    auto hue2rgb = [](double p, double q, double t) -> double
    {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
        if (t < 1.0 / 2.0) return q;
        if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
        return p;
    };

    double r, g, b;
    if (s == 0)
    {
        r = g = b = l;
    }
    else
    {
        double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        double p = 2 * l - q;
        r = hue2rgb(p, q, h + 1.0 / 3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0 / 3.0);
    }

    BYTE ri = static_cast<BYTE>(std::clamp(r * 255.0, 0.0, 255.0));
    BYTE gi = static_cast<BYTE>(std::clamp(g * 255.0, 0.0, 255.0));
    BYTE bi = static_cast<BYTE>(std::clamp(b * 255.0, 0.0, 255.0));
    return Gdiplus::Color(255, ri, gi, bi);
}

Gdiplus::Color LabelToColor(const std::wstring& label, size_t totalCount)
{
    if (label.length() < 2) return Gdiplus::Color(255, 255, 165, 0);

    int idx = (label[0] - L'A') * 26 + (label[1] - L'A');
    double hue = 0.0;
    if (totalCount > 1)
        hue = static_cast<double>(idx) / static_cast<double>(totalCount);
    else
        hue = 0.0;

    double sat = 0.80;
    double lig = 0.80;
    return HslToColor(hue, sat, lig);
}

Gdiplus::Color GetTextColorForBackground(Gdiplus::Color bg)
{
    double luminance = (0.299 * bg.GetR() + 0.587 * bg.GetG() + 0.114 * bg.GetB());
    return luminance > 128 ? Gdiplus::Color(255, 0, 0, 0) : Gdiplus::Color(255, 255, 255, 255);
}

void AddRoundRect(Gdiplus::GraphicsPath& path, int x, int y, int w, int h, int radius)
{
    int d = radius * 2;
    d = min(d, w);
    d = min(d, h);
    int r = d / 2;

    path.AddArc(x, y, d, d, 180, 90);
    path.AddLine(x + r, y, x + w - r, y);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddLine(x + w, y + r, x + w, y + h - r);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddLine(x + w - r, y + h, x + r, y + h);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.AddLine(x, y + h - r, x, y + r);
    path.CloseFigure();
}

} // namespace

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int winW = clientRect.right - clientRect.left;
        int winH = clientRect.bottom - clientRect.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, winW, winH);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        PatBlt(memDC, 0, 0, winW, winH, BLACKNESS);

        {
            using namespace Gdiplus;
            Graphics graphics(memDC);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

            Font font(L"Segoe UI", static_cast<Gdiplus::REAL>(g_fontSize), FontStyleBold, UnitPoint);

            int vLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int vBottom = vTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);

            for (const auto& elem : g_elements)
            {
                if (elem.label.empty()) continue;

                bool matched = (g_firstKey == 0) || (elem.label[0] == g_firstKey);

                Color baseColor = LabelToColor(elem.label, g_elements.size());
                SolidBrush textBrush(matched ? GetTextColorForBackground(baseColor) : Color(100, 200, 200, 200));
                SolidBrush bgBrush(matched ? Color(g_bgAlpha, baseColor.GetR(), baseColor.GetG(), baseColor.GetB()) : Color(30, 80, 80, 80));
                Pen borderPen(matched ? Color(255, baseColor.GetR(), baseColor.GetG(), baseColor.GetB()) : Color(40, 100, 100, 100), 2);
                Pen tagBorderPen(matched ? Color(255, baseColor.GetR(), baseColor.GetG(), baseColor.GetB()) : Color(40, 100, 100, 100), 1);

                graphics.DrawRectangle(&borderPen,
                    static_cast<INT>(elem.rect.left), static_cast<INT>(elem.rect.top),
                    static_cast<INT>(elem.rect.right - elem.rect.left - 1),
                    static_cast<INT>(elem.rect.bottom - elem.rect.top - 1));

                RectF layoutRect;
                graphics.MeasureString(elem.label.c_str(), -1, &font, PointF(0, 0), &layoutRect);
                int padX = 6;
                int padY = 3;
                int tagW = (int)(layoutRect.Width + 0.5f) + padX * 2;
                int tagH = (int)(layoutRect.Height + 0.5f) + padY * 2;
                int cornerRadius = g_cornerRadius;

                int centerX = (elem.rect.left + elem.rect.right) / 2;
                int tagX = centerX - tagW / 2;
                int tagY;

                if (g_tagPosition == L"top")
                {
                    tagY = elem.rect.top - tagH;
                    if (tagY < vTop)
                        tagY = elem.rect.bottom;
                }
                else
                {
                    tagY = elem.rect.bottom;
                    if (tagY + tagH > vBottom)
                        tagY = elem.rect.top - tagH;
                }

                if (tagX < vLeft)
                {
                    tagX = vLeft + 2;
                }

                GraphicsPath path;
                AddRoundRect(path, tagX, tagY, tagW, tagH, cornerRadius);
                graphics.FillPath(&bgBrush, &path);
                graphics.DrawPath(&tagBorderPen, &path);

                StringFormat format;
                format.SetAlignment(StringAlignmentCenter);
                format.SetLineAlignment(StringAlignmentCenter);
                RectF textRect((REAL)tagX, (REAL)tagY, (REAL)tagW, (REAL)tagH);
                graphics.DrawString(elem.label.c_str(), -1, &font, textRect, &format, &textBrush);
            }

            // 右下角绘制当前鼠标模式
            const wchar_t* modeText = L"[LMB]";
            if (g_currentMode == MouseMode::Right) modeText = L"[RMB]";
            else if (g_currentMode == MouseMode::Middle) modeText = L"[MMB]";
            else if (g_currentMode == MouseMode::LeftDoubleClick) modeText = L"[DBL]";

            Color modeColor(255, 255, 165, 0);
            SolidBrush modeTextBrush(Color(255, 0, 0, 0));
            SolidBrush modeBgBrush(Color(220, 255, 165, 0));
            Pen modeBorderPen(Color(255, 255, 165, 0));

            RectF modeLayout;
            graphics.MeasureString(modeText, -1, &font, PointF(0, 0), &modeLayout);
            int modeW = (int)(modeLayout.Width + 0.5f) + 10;
            int modeH = (int)(modeLayout.Height + 0.5f) + 6;
            int modeX = winW - modeW - 10;
            int modeY = winH - modeH - 10;

            GraphicsPath modePath;
            AddRoundRect(modePath, modeX, modeY, modeW, modeH, g_cornerRadius);
            graphics.FillPath(&modeBgBrush, &modePath);
            graphics.DrawPath(&modeBorderPen, &modePath);

            StringFormat modeFormat;
            modeFormat.SetAlignment(StringAlignmentCenter);
            modeFormat.SetLineAlignment(StringAlignmentCenter);
            RectF modeTextRect((REAL)modeX, (REAL)modeY, (REAL)modeW, (REAL)modeH);
            graphics.DrawString(modeText, -1, &font, modeTextRect, &modeFormat, &modeTextBrush);
        }

        BitBlt(hdc, 0, 0, winW, winH, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        g_hOverlayWnd = nullptr;
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static HWND CreateOverlayWindow()
{
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.lpfnWndProc = OverlayWndProc;
        wcex.hInstance = hInst;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.lpszClassName = L"NexusOverlay";
        RegisterClassExW(&wcex);
        classRegistered = true;
    }

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HWND hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"NexusOverlay", L"NexusOverlay",
        WS_POPUP,
        x, y, cx, cy,
        nullptr, nullptr, hInst, nullptr);

    if (hWnd)
    {
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    }
    return hWnd;
}

void ShowOverlay(const std::vector<ClickableElement>& elements, MouseMode mode)
{
    g_elements = elements;
    g_currentMode = mode;
    g_firstKey = 0;

    if (!g_hOverlayWnd)
    {
        g_hOverlayWnd = CreateOverlayWindow();
    }

    if (g_hOverlayWnd)
    {
        SetWindowPos(g_hOverlayWnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
        InvalidateRect(g_hOverlayWnd, nullptr, FALSE);
    }
}

void HideOverlay()
{
    if (g_hOverlayWnd && IsWindow(g_hOverlayWnd))
    {
        ShowWindow(g_hOverlayWnd, SW_HIDE);
    }
    g_elements.clear();
    g_firstKey = 0;
}

void SetOverlayMouseMode(MouseMode mode)
{
    g_currentMode = mode;
    if (g_hOverlayWnd && IsWindow(g_hOverlayWnd) && IsWindowVisible(g_hOverlayWnd))
    {
        InvalidateRect(g_hOverlayWnd, nullptr, FALSE);
    }
}

void SetOverlayFirstKey(char key)
{
    g_firstKey = key;
    if (g_hOverlayWnd && IsWindow(g_hOverlayWnd) && IsWindowVisible(g_hOverlayWnd))
    {
        InvalidateRect(g_hOverlayWnd, nullptr, FALSE);
    }
}

void SetOverlayStyle(int fontSize, int bgAlpha, int cornerRadius)
{
    g_fontSize = fontSize;
    if (g_fontSize < 8) g_fontSize = 8;
    if (g_fontSize > 48) g_fontSize = 48;

    g_bgAlpha = bgAlpha;
    if (g_bgAlpha < 0) g_bgAlpha = 0;
    if (g_bgAlpha > 255) g_bgAlpha = 255;

    g_cornerRadius = cornerRadius;
    if (g_cornerRadius < 0) g_cornerRadius = 0;
    if (g_cornerRadius > 20) g_cornerRadius = 20;
}

void SetOverlayTagPosition(const std::wstring& pos)
{
    g_tagPosition = pos;
    if (g_tagPosition != L"top" && g_tagPosition != L"bottom")
        g_tagPosition = L"bottom";
}
