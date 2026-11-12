#include "WinNative.h"

#include <QWidget>

#include <windows.h>
#include <windowsx.h>

namespace WinNative {

namespace {

const wchar_t kMutexName[]    = L"Local\\Polaris.SingleInstance";
const wchar_t kActivateName[] = L"Polaris.Activate";

UINT activateMessage()
{
    static const UINT msg = RegisterWindowMessageW(kActivateName);
    return msg;
}

// Qt::Key -> 虚拟键码；不支持的键返回 0。
UINT toVirtualKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return UINT('A' + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return UINT('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return UINT(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
    case Qt::Key_Space:     return VK_SPACE;
    case Qt::Key_Up:        return VK_UP;
    case Qt::Key_Down:      return VK_DOWN;
    case Qt::Key_Left:      return VK_LEFT;
    case Qt::Key_Right:     return VK_RIGHT;
    case Qt::Key_Home:      return VK_HOME;
    case Qt::Key_End:       return VK_END;
    case Qt::Key_PageUp:    return VK_PRIOR;
    case Qt::Key_PageDown:  return VK_NEXT;
    case Qt::Key_Insert:    return VK_INSERT;
    case Qt::Key_Delete:    return VK_DELETE;
    case Qt::Key_Tab:       return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter:     return VK_RETURN;
    case Qt::Key_Escape:    return VK_ESCAPE;
    case Qt::Key_Minus:     return VK_OEM_MINUS;
    case Qt::Key_Equal:
    case Qt::Key_Plus:      return VK_OEM_PLUS;
    case Qt::Key_Comma:     return VK_OEM_COMMA;
    case Qt::Key_Period:    return VK_OEM_PERIOD;
    case Qt::Key_Slash:     return VK_OEM_2;
    case Qt::Key_Semicolon: return VK_OEM_1;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    case Qt::Key_BracketLeft:  return VK_OEM_4;
    case Qt::Key_Backslash:    return VK_OEM_5;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Apostrophe:   return VK_OEM_7;
    default:                return 0;
    }
}

HWND hwndOf(QWidget *window)
{
    return reinterpret_cast<HWND>(window->winId());
}

} // namespace

void setTopmost(QWidget *window, bool on)
{
    SetWindowPos(hwndOf(window), on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void bringToFront(QWidget *window)
{
    HWND hwnd = hwndOf(window);
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}

bool registerHotkey(QWidget *window, int id, int key, Qt::KeyboardModifiers modifiers,
                    bool allowRepeat)
{
    const UINT vk = toVirtualKey(key);
    if (vk == 0)
        return false;
    UINT mods = allowRepeat ? 0 : MOD_NOREPEAT;
    if (modifiers & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (modifiers & Qt::AltModifier)     mods |= MOD_ALT;
    if (modifiers & Qt::ShiftModifier)   mods |= MOD_SHIFT;
    if (modifiers & Qt::MetaModifier)    mods |= MOD_WIN;
    return RegisterHotKey(hwndOf(window), id, mods, vk) != 0;
}

void unregisterHotkey(QWidget *window, int id)
{
    UnregisterHotKey(hwndOf(window), id);
}

bool acquireSingleInstance()
{
    // 句柄故意不关闭：进程存活期间一直持有，退出时由系统回收。
    HANDLE h = CreateMutexW(nullptr, FALSE, kMutexName);
    return h != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

void notifyExistingInstance()
{
    // 允许对方把自己的窗口设为前台，然后广播一条注册消息。
    AllowSetForegroundWindow(ASFW_ANY);
    PostMessageW(HWND_BROADCAST, activateMessage(), 0, 0);
}

void setupFramelessWindow(QWidget *window)
{
    HWND hwnd = hwndOf(window);
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

bool handleNativeEvent(QWidget *window, const QByteArray &eventType, void *message,
                       long *result, int borderWidth, Events *events)
{
    if (eventType != "windows_generic_MSG")
        return false;

    MSG *msg = static_cast<MSG *>(message);

    if (msg->message == activateMessage()) {
        events->activateRequest = true;
        *result = 0;
        return true;
    }

    switch (msg->message) {
    case WM_HOTKEY:
        events->hotkeyId = int(msg->wParam);
        *result = 0;
        return true;

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
