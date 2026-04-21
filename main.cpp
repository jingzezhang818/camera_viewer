#include "widget.h"

#include <QApplication>

// 程序入口：初始化Qt应用对象，创建主窗口并进入事件循环。
int main(int argc, char *argv[])
{
    // QApplication负责事件分发、窗口系统交互等全局能力。
    QApplication a(argc, argv);

    // 主界面对象，封装XDMA设备连接、数据接收与图像预览。
    Widget w;
    w.show();

    // 启动事件循环，直到应用退出。
    return a.exec();
}
