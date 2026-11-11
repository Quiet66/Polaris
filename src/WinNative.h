#pragma once
// 所有 Win32 专用调用都集中在这里；其他文件不得包含 windows.h。

#include <QByteArray>

class QWidget;

namespace WinNative {

// 无边框窗口的原生设置：恢复 WS_THICKFRAME 等样式，
// 让系统提供边缘缩放、最大化/最小化动画和 Aero Snap。
void setupFramelessWindow(QWidget *window);

// 处理 QWidget::nativeEvent 转来的消息。
// borderWidth：边缘缩放热区宽度（设备无关像素）。
// 返回 true 表示消息已处理，*result 为返回值。
bool handleNativeEvent(QWidget *window, const QByteArray &eventType,
                       void *message, long *result, int borderWidth);

} // namespace WinNative
