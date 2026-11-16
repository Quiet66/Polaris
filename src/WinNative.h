#pragma once
// 所有 Win32 专用调用都集中在这里；其他文件不得包含 windows.h。

#include <QByteArray>
#include <QString>
#include <Qt>

class QWidget;

namespace WinNative {

// ---- 无边框窗口

// 恢复 WS_THICKFRAME 等样式，让系统提供边缘缩放、最大化/最小化动画和 Aero Snap。
void setupFramelessWindow(QWidget *window);

// nativeEvent 里除了返回值之外需要上层处理的事件。
struct Events
{
    int  hotkeyId = -1;         // 收到 WM_HOTKEY 时为注册时的 id
    bool activateRequest = false;   // 另一个实例启动，要求唤出本窗口
};

// 处理 QWidget::nativeEvent 转来的消息。
// borderWidth：边缘缩放热区宽度（设备无关像素）。
// 返回 true 表示消息已处理，*result 为返回值。
bool handleNativeEvent(QWidget *window, const QByteArray &eventType, void *message,
                       long *result, int borderWidth, Events *events);

// ---- 置顶与前台

// 用 SetWindowPos 切换置顶，不重建窗口。
void setTopmost(QWidget *window, bool on);

// 把窗口带到前台并激活（从托盘或全局快捷键唤出时用）。
void bringToFront(QWidget *window);

// ---- 全局快捷键（RegisterHotKey）

// key 是 Qt::Key，不含修饰键。不支持的键或已被占用时返回 false。
bool registerHotkey(QWidget *window, int id, int key, Qt::KeyboardModifiers modifiers,
                    bool allowRepeat);
void unregisterHotkey(QWidget *window, int id);

// ---- 崩溃记录

// 程序因未处理的异常崩溃时，往 logPath 追加一行（时间、异常代码、地址），然后照常结束。
void installCrashLogger(const QString &logPath);

// ---- 单实例（命名互斥量）

// 第一个实例返回 true 并一直持有互斥量；已有实例时返回 false。
bool acquireSingleInstance();
// 通知已有实例唤出窗口。
void notifyExistingInstance();

} // namespace WinNative
