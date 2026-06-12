#include "mainwindow.h"
#include "lockscreen.h"
#include "bridge.h"
#include <QTimer>
#include <QScreen>
#include <QProcessEnvironment>
#include <QProcess>
#include <QFile>
#include <unistd.h>
#include <QtWidgets/QApplication>

/**
 * @brief 主程序入口
 *
 * 程序流程：
 *   锁屏界面 -> 绘制正确图案解锁 -> 主界面(摄像头+按钮) -> 点击锁屏按钮 -> 锁屏界面
 *
 * 嵌入式 Linux 注意事项：
 *   - LCD 分辨率: 800x480 (蓝框屏)
 *   - 触摸屏事件由 Qt 的 Linux Input 插件自动处理
 *   - 使用 QStackedWidget 管理锁屏/主界面切换，避免 showFullScreen 导致的尺寸计算错误
 */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GEC6818SmartGate");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("GEC6818");

    // 获取屏幕分辨率 800x480
    QScreen *screen = app.primaryScreen();
    QSize screenSize = screen->geometry().size();
    printf("屏幕分辨率: %dx%d\n", screenSize.width(), screenSize.height());

    // 正确密码：1->2->3->6->9 (索引 0->1->2->5->8)
    QVector<int> correctPattern = {0, 1, 2, 5, 8};

    // ===== 用 QStackedWidget 管理两个界面 =====
    QStackedWidget *stack = new QStackedWidget();
    stack->setStyleSheet("QStackedWidget { background-color: #1a1a2e; }");

    // ===== 锁屏界面 =====
    LockScreen *lockScreen = new LockScreen(stack);
    lockScreen->setCorrectPattern(correctPattern);
    stack->addWidget(lockScreen);

    // ===== 主界面 =====
    MainWindow *mainWindow = new MainWindow(stack);
    stack->addWidget(mainWindow);

    // ===== 桥接类：解决信号槽参数不匹配 =====
    // unlocked() 无参数，stack->setCurrentIndex(int) 需要一个参数
    // Bridge 类提供 showMainWindow() 槽，内部调用 setCurrentIndex(1)
    Bridge *bridge = new Bridge(stack, lockScreen);
    
    // 解锁成功 -> 切换到主界面
    QObject::connect(lockScreen, SIGNAL(unlocked()), bridge, SLOT(showMainWindow()));

    // 锁屏请求 -> 回到锁屏界面
    QObject::connect(mainWindow, SIGNAL(lockRequested()), bridge, SLOT(showLockScreen()));

    // 启动时显示锁屏界面
    lockScreen->show();

    // 全屏显示 stacked widget
    stack->showFullScreen();

    // 播放启动动画视频
    QString startVideo = "/frog/start.MP4";
    if (QFile::exists(startVideo)) {
        printf("播放启动动画...\n");
        stack->hide();

        // 同时启动视频(mplayer 静音) + 音频(madplay)
        QProcess videoProc;
        QProcess audioProc;
        videoProc.start("mplayer", QStringList()
                        << "-vo" << "fbdev"
                        << "-ao" << "null"
                        << "-quiet"
                        << startVideo);
        audioProc.start("madplay", QStringList() << "/frog/start.mp3");

        if (!videoProc.waitForFinished(8000)) {
            videoProc.kill();
            videoProc.waitForFinished(1000);
        }
        audioProc.kill();
        audioProc.waitForFinished(500);

        usleep(200000);
        QProcess::execute("dd", QStringList()
                          << "if=/dev/zero" << "of=/dev/fb0"
                          << "bs=1536000" << "count=1");
        usleep(100000);
        printf("启动动画结束\n");
        stack->show();
        stack->repaint();
        app.processEvents();
    }

    printf("=== 智能门禁系统启动 ===\n");
    printf("正确密码: 1 -> 2 -> 3 -> 6 -> 9\n");
    printf("按提示绘制图案即可解锁\n");

    return app.exec();
}
