#include "WinNative.h"

#include <QWidget>

#include <windows.h>
#include <windowsx.h>

namespace WinNative {

void setupFramelessWindow(QWidget *window)
{
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    // WS_THICKFRAME：系统边缘缩放与 Aero Snap；WS_MAXIMIZEBOX/WS_MINIMIZEBOX：最大化、最小化。
    // 不加 WS_CAPTION，避免 Win7 上露出标题栏。
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
    SetWindowLongW(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static LRESULT hitTest(HWND hwnd, LPARAM lParam, int border)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);

    if (IsZoomed(hwnd))
        return HTCLIENT;

    const bool left   = x <  rc.left   + border;
    const bool right  = x >= rc.right  - border;
    const bool top    = y <  rc.top    + border;
    const bool bottom = y >= rc.bottom - border;

    if (top && left)     return HTTOPLEFT;
    if (top && right)    return HTTOPRIGHT;
    if (bottom && left)  return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left)            return HTLEFT;
    if (right)           return HTRIGHT;
    if (top)             return HTTOP;
    if (bottom)          return HTBOTTOM;
    return HTCLIENT;
}

bool handleNativeEvent(QWidget *window, const QByteArray &eventType,
                       void *message, long *result, int borderWidth)
{
    if (eventType != "windows_generic_MSG")
        return false;

    MSG *msg = static_cast<MSG *>(message);
    switch (msg->message) {
    case WM_NCCALCSIZE:
        // 整个窗口都是客户区：去掉 WS_THICKFRAME 带来的可见边框。
        if (msg->wParam == TRUE) {
            if (IsZoomed(msg->hwnd)) {
                // 最大化时系统会把窗口外扩一圈边框，这里收回到工作区，避免内容溢出屏幕。
                HMONITOR mon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(mon, &mi)) {
                    NCCALCSIZE_PARAMS *p = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                    p->rgrc[0] = mi.rcWork;
                }
            }
            *result = 0;
            return true;
        }
        return false;

    case WM_NCHITTEST: {
        const int border = qRound(borderWidth * window->devicePixelRatioF());
        const LRESULT hit = hitTest(msg->hwnd, msg->lParam, border);
        if (hit == HTCLIENT)
            return false;       // 交给 Qt 按普通客户区处理
        *result = static_cast<long>(hit);
        return true;
    }

    default:
        return false;
    }
}

} // namespace WinNative
