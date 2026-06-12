#include "touchscreen.h"
#include <fcntl.h>

TouchScreen::TouchScreen()
    : tc_fd_(-1), raw_x_(0), raw_y_(0)
{
}

TouchScreen::~TouchScreen()
{
    close();
}

bool TouchScreen::init()
{
    // 自动尝试 event0 / event1，适配不同设备节点
    tc_fd_ = open("/dev/input/event0", O_RDONLY);
    if (tc_fd_ == -1) {
        perror("open event0 failed");
        tc_fd_ = open("/dev/input/event1", O_RDONLY);
        if (tc_fd_ == -1) {
            perror("open event1 failed, 触摸屏设备打开失败！");
            return false;
        }
    }
    printf("触摸屏初始化成功 (fd=%d)\n", tc_fd_);
    return true;
}

void TouchScreen::convertCoordinate(int raw_x, int raw_y, int &x, int &y)
{
#ifdef BLACK_SCREEN
    // 四舍五入，修复坐标偏移
    x = (raw_x * LCD_W + TOUCH_X_MAX / 2) / TOUCH_X_MAX;
    y = (raw_y * LCD_H + TOUCH_Y_MAX / 2) / TOUCH_Y_MAX;
#else
    // 蓝框屏：直接映射
    x = raw_x;
    y = raw_y;
#endif
}

int TouchScreen::getTouch(int &x, int &y)
{
    if (tc_fd_ < 0) {
        return 0;
    }

    struct input_event ie;
    int start_x = -1, start_y = -1;
    int end_x = -1, end_y = -1;
    int raw_x = 0, raw_y = 0;

    // 循环读取触摸事件，直到触摸抬起
    while (1) {
        ssize_t n = read(tc_fd_, &ie, sizeof(ie));
        if (n < (ssize_t)sizeof(ie)) {
            perror("read touch event failed");
            return 0;
        }

        // 解析触摸绝对坐标
        if (ie.type == EV_ABS) {
            if (ie.code == ABS_X) {
                raw_x = ie.value;
            }
            if (ie.code == ABS_Y) {
                raw_y = ie.value;
            }
            // 坐标换算
            convertCoordinate(raw_x, raw_y, x, y);
        }

        // 触摸按下/抬起事件
        if (ie.type == EV_KEY && ie.code == BTN_TOUCH) {
            if (ie.value == 1) {
                // 按下：记录起点坐标
                if (start_x == -1 || start_y == -1) {
                    start_x = x;
                    start_y = y;
                }
            } else if (ie.value == 0) {
                // 抬起：记录终点坐标，退出循环
                end_x = x;
                end_y = y;
                break;
            }
        }
    }

    // 计算坐标偏移量（参考文档：阈值30判定点击 vs 滑动）
    int diff_x = end_x - start_x;
    int diff_y = end_y - start_y;
    int abs_x = abs(diff_x);
    int abs_y = abs(diff_y);

    // 判定逻辑：先判断点击，再判断滑动
    if (abs_x < 30 && abs_y < 30) {
        return 'h';  // hit - 点击
    }

    // 上下滑动 (Y轴偏移更大)
    if (abs_y > abs_x) {
        if (diff_y < 0) return 'u';  // up
        else            return 'd';  // down
    }
    // 左右滑动 (X轴偏移更大)
    else {
        if (diff_x < 0) return 'l';  // left
        else            return 'r';  // right
    }

    return 0;
}

int TouchScreen::pollTouch(int &x, int &y)
{
    if (tc_fd_ < 0) return 0;

    struct input_event ie;
    // 非阻塞读取
    int flags = fcntl(tc_fd_, F_GETFL);
    fcntl(tc_fd_, F_SETFL, flags | O_NONBLOCK);

    ssize_t n = read(tc_fd_, &ie, sizeof(ie));
    if (n < (ssize_t)sizeof(ie)) {
        // 恢复阻塞模式
        fcntl(tc_fd_, F_SETFL, flags);
        return 0;
    }

    // 恢复阻塞模式
    fcntl(tc_fd_, F_SETFL, flags);

    int raw_x = 0, raw_y = 0;
    if (ie.type == EV_ABS) {
        if (ie.code == ABS_X) raw_x = ie.value;
        if (ie.code == ABS_Y) raw_y = ie.value;
        convertCoordinate(raw_x, raw_y, x, y);
        return 1;
    }

    return 0;
}

void TouchScreen::close()
{
    if (tc_fd_ >= 0) {
        ::close(tc_fd_);
        tc_fd_ = -1;
    }
}