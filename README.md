# 图形解锁锁屏界面 - GEC6818 开发板

基于 Qt5 的嵌入式 Linux 图形解锁应用。参考粤嵌 GEC6818 实训教程触摸屏编程技术。

## 项目结构

```
codex/
├── CMakeLists.txt          # CMake 构建配置
├── README.md
├── include/
│   ├── touchscreen.h/cpp   # 触摸屏驱动层 (/dev/input/event0 → input_event)
│   ├── patternlock.h/cpp   # 图形解锁组件 (3x3 网格, 触摸绘制)
│   ├── lockscreen.h/cpp    # 锁屏界面
│   └── mainwindow.h/cpp    # 主界面
└── src/
    └── main.cpp            # 程序入口
```

## 功能

| 界面 | 功能 |
|---|---|
| 锁屏界面 | 3×3 图形解锁，触摸拖动绘制图案，至少4个点 |
| 主界面 | 深色主题 + 锁屏按钮 |

**默认密码：1→2→3→6→9**

## 编译

### PC 调试编译 (x86_64)

```bash
cd codex
mkdir build && cd build
cmake ..
make -j4
./patternlock
```

### 交叉编译到 GEC6818 开发板 (ARM)

```bash
cd codex
mkdir build-arm && cd build-arm
cmake .. \
    -DCMAKE_C_COMPILER=arm-linux-gcc \
    -DCMAKE_CXX_COMPILER=arm-linux-g++ \
    -DCMAKE_PREFIX_PATH=/opt/qte    # ← 改为你 ARM 版 Qt5 的路径
make -j4
```

**关键：`-DCMAKE_PREFIX_PATH` 必须指向 ARM 版 Qt5 的根目录（包含 `lib/`、`include/`、`mkspecs/` 的那个目录）。**

### 如何找到 ARM 版 Qt5 路径

在你的 Linux 虚拟机上运行：
```bash
# 找到 qmake 位置
which qmake

# 或者找常见位置
ls /opt/qte/bin/qmake
ls /usr/local/qt5arm/bin/qmake
ls /usr/lib/aarch64-linux-gnu/qt5/bin/qmake

# 看 qmake 是不是 ARM 版
file /opt/qte/bin/qmake
# 应该显示: ELF 64-bit LSB executable, ARM aarch64
```

**找到后把路径填到 `-DCMAKE_PREFIX_PATH=上面找到的路径的父目录`**

比如 `qmake` 在 `/opt/qte/bin/qmake`，那么 `CMAKE_PREFIX_PATH=/opt/qte`

## 部署到开发板

```bash
# 通过 scp 或 NFS 拷贝
scp build-arm/patternlock root@<开发板IP>:/usr/bin/

# 在开发板上运行
ssh root@<开发板IP>
./patternlock

# 如果屏幕不显示，指定 Qt 平台插件
QT_QPA_PLATFORM=linuxfb ./patternlock
# 或
QT_QPA_PLATFORM=eglfs ./patternlock
```

## 技术细节

触摸屏事件映射：
- 文档 `EV_ABS` → Qt `QMouseEvent`
- 文档 `ABS_X/ABS_Y` → `event->pos().x()/y()`
- 文档 `BTN_TOUCH` → `mousePressEvent/Release`
- 文档 坐标换算 `raw * LCD / TOUCH_MAX` → Qt 自动缩放
- 文档 阈值30判定点击 → `dotRadius_ = 25`