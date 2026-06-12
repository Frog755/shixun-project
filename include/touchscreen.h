#ifndef TOUCHSCREEN_H
#define TOUCHSCREEN_H

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================
// 参考文档配置：蓝框屏(800x480, 无需换算) vs 黑框屏(1024x600, 需换算)
// GEC6818 开发板通常配 7 寸 LCD，蓝框屏 800x480 居多
// 如果实际屏幕是 1024x600，取消下面 #define BLUE_SCREEN 的注释
// ============================================================
#define BLUE_SCREEN          // 蓝框屏：触摸分辨率 800x480，无需坐标换算
// #define BLACK_SCREEN      // 黑框屏：触摸分辨率 1024x600，需要坐标换算

#define LCD_W       800
#define LCD_H       480

#ifdef BLACK_SCREEN
    #define TOUCH_X_MAX   1024
    #define TOUCH_Y_MAX   600
#else
    #define TOUCH_X_MAX   800
    #define TOUCH_Y_MAX   480
#endif

/**
 * @brief 触摸屏底层驱动类
 * 参考文档：嵌入式 Linux 触摸屏输入事件处理
 * - 打开 /dev/input/eventX
 * - 读取 input_event 结构体
 * - 解析 EV_ABS (坐标) 和 EV_KEY (触摸状态)
 */
class TouchScreen {
public:
    TouchScreen();
    ~TouchScreen();

    /**
     * @brief 初始化触摸屏，打开设备文件
     * @return true 成功, false 失败
     */
    bool init();

    /**
     * @brief 阻塞读取一次触摸事件（按下→抬起）
     * @param x 传出：抬起时的屏幕坐标 X
     * @param y 传出：抬起时的屏幕坐标 Y
     * @return 操作类型: 'hit'(点击), 'up', 'down', 'left', 'right', 或 0(未知)
     */
    int getTouch(int &x, int &y);

    /**
     * @brief 非阻塞轮询方式读取触摸数据
     * @param x 传出坐标 X
     * @param y 传出坐标 Y
     * @return 0=无事件, 1=有触摸事件
     */
    int pollTouch(int &x, int &y);

    /**
     * @brief 关闭触摸屏设备
     */
    void close();

private:
    int tc_fd_;                    // 触摸屏文件描述符
    int raw_x_;                    // 原始触摸 X
    int raw_y_;                    // 原始触摸 Y

    /**
     * @brief 坐标换算：将触摸原始坐标映射到 LCD 屏幕坐标
     */
    void convertCoordinate(int raw_x, int raw_y, int &x, int &y);
};

#endif // TOUCHSCREEN_H