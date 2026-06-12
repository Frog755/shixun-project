# GEC6818 无人车库管理系统

基于 Qt5 + OpenCV + RFID 的嵌入式 Linux 无人车库系统，运行在粤嵌 GEC6818 开发板上。

## 功能列表

| 功能 | 说明 |
|------|------|
| 图形解锁 | 3x3 网格图案解锁，密码：1→2→3→6→9 |
| 车辆入库 | 摄像头抓拍 → ALPR 车牌识别 → 确认入库 |
| 车辆出库 | 摄像头抓拍 → ALPR 车牌识别 → 计算停车费 → 确认出库 |
| RFID 自动出入库 | 刷卡自动识别车辆，根据数据库判断入库/出库 |
| 车位计数 | 实时显示当前库内车辆数 |
| 进出记录 | 查看最近 20 条进出记录 |
| 超时锁屏 | 1 分钟无操作自动锁屏 |
| 开机动画 | 启动时播放视频动画 |

## 项目结构

```
project/
├── CMakeLists.txt              # CMake 构建配置
├── toolchain-gec6818.cmake     # ARM 交叉编译工具链
├── build.sh                    # 一键编译脚本
├── README.md
├── DEPLOY.md                   # 部署文档
├── include/
│   ├── rfidreader.h            # RFID 读取器线程类
│   ├── patternlock.h           # 图形解锁组件
│   ├── lockscreen.h            # 锁屏界面
│   ├── mainwindow.h            # 主界面（摄像头+按钮+RFID）
│   ├── bridge.h                # 信号槽桥接类
│   └── touchscreen.h           # 触摸屏驱动
├── src/
│   ├── rfidreader.cpp          # RFID 串口通信实现
│   ├── patternlock.cpp         # 图形解锁逻辑
│   ├── lockscreen.cpp          # 锁屏界面实现
│   ├── mainwindow.cpp          # 主界面业务逻辑
│   ├── main.cpp                # 程序入口
│   ├── touchscreen.cpp         # 触摸屏驱动实现
│   └── alpr_main.cpp           # ALPR 车牌识别子程序
└── build-arm/                  # ARM 编译输出目录
    └── Gec6818SmartGate        # 编译产物
```

## 编译环境

### 虚拟机要求

- Ubuntu 16.04/18.04（推荐）
- 已安装 VMware Tools 或 Open VM Tools
- 共享目录已配置：`/mnt/hgfs/shixun/`

### 依赖项

| 依赖 | 路径 | 说明 |
|------|------|------|
| ARM 交叉编译器 | `/usr/local/arm/5.4.0/` | arm-linux-gcc/g++ 5.4.0 |
| Qt-Embedded 5.7.0 | `/mnt/hgfs/shixun/Qt-Embedded-5.7.0/` | ARM 版 Qt 库 |
| OpenCV 3.4 | `/mnt/hgfs/shixun/09_opencv/` | ARM 版 OpenCV 库 |
| HyperLPR | `/mnt/hgfs/shixun/09_opencv/原资料/zeusees-HyperLPR-master源码包.zip` | 车牌识别源码 |
| moc | `/usr/lib/x86_64-linux-gnu/qt5/bin/moc` | Qt 元对象编译器（宿主机） |

## 编译步骤

### 1. 进入项目目录

```bash
cd /mnt/hgfs/shixun/project
```

### 2. 清理旧的编译缓存（如有）

```bash
rm -rf build-arm
```

### 3. 创建编译目录并编译

```bash
mkdir -p build-arm && cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-gec6818.cmake
make -j4
```

### 4. 确认编译成功

编译成功后，`build-arm/` 目录下会生成 `Gec6818SmartGate` 可执行文件：

```bash
ls -lh build-arm/Gec6818SmartGate
```

## 打包部署

### 打包目录结构

将编译产物和依赖库打包到部署目录：

```
D:\frog\Downloads\shixun\deploy_patternlock_v2\
├── Gec6818SmartGate        # 主程序
├── alpr/                   # 车牌识别模块
│   ├── alpr                # ALPR 可执行文件
│   └── model/              # AI 模型文件
├── lib/                    # OpenCV 动态库
│   ├── libopencv_core.so.3.4
│   ├── libopencv_imgproc.so.3.4
│   ├── libopencv_imgcodecs.so.3.4
│   └── ...
└── start.MP4               # 开机动画视频（可选）
```

### 手动打包步骤

1. **复制主程序**
   ```bash
   cp build-arm/Gec6818SmartGate /mnt/hgfs/shixun/deploy_patternlock_v2/
   ```

2. **复制 ALPR 模块**
   ```bash
   # 如果使用 build.sh 编译，ALPR 会自动打包
   # 手动打包需要：
   cp ~/HyperLPR/Prj-Linux/lpr/build-arm/alpr /mnt/hgfs/shixun/deploy_patternlock_v2/alpr/
   cp -r ~/HyperLPR/Prj-Linux/lpr/model /mnt/hgfs/shixun/deploy_patternlock_v2/alpr/
   ```

3. **复制 OpenCV 动态库**
   ```bash
   for lib in libopencv_core.so.3.4 libopencv_imgproc.so.3.4 \
              libopencv_imgcodecs.so.3.4 libopencv_videoio.so.3.4; do
       cp /mnt/hgfs/shixun/09_opencv/lib/$lib /mnt/hgfs/shixun/deploy_patternlock_v2/lib/
   done
   ```

## 部署到开发板

### 方法一：通过共享目录（推荐）

开发板可以直接访问 VMware 共享目录：

```bash
# 在开发板上
cd /mnt/hgfs/shixun/deploy_patternlock_v2
chmod +x Gec6818SmartGate alpr/alpr
./Gec6818SmartGate
```

### 方法二：通过 U 盘

1. **在虚拟机上复制到 U 盘**
   ```bash
   cp -r /mnt/hgfs/shixun/deploy_patternlock_v2 /media/$USER/<U盘名>/
   sync
   ```

2. **在开发板上挂载 U 盘并复制**
   ```bash
   mkdir -p /mnt/usb
   mount /dev/sda1 /mnt/usb
   mkdir -p /opt/deploy
   cp -r /mnt/usb/deploy_patternlock_v2/* /opt/deploy/
   chmod +x /opt/deploy/Gec6818SmartGate /opt/deploy/alpr/alpr
   umount /mnt/usb
   ```

3. **运行程序**
   ```bash
   cd /opt/deploy
   ./Gec6818SmartGate
   ```

## 运行说明

### 启动程序

```bash
cd /path/to/deploy_patternlock_v2
chmod +x Gec6818SmartGate alpr/alpr
./Gec6818SmartGate
```

### 程序流程

1. **开机动画**（如果存在 `start.MP4`）
   - 播放 8 秒开机动画视频
   - 自动清屏进入锁屏界面

2. **锁屏界面**
   - 绘制图案 1→2→3→6→9 解锁
   - 点击"显示/隐藏密码"查看提示

3. **主界面**
   - 左侧：摄像头实时预览
   - 右侧：操作按钮 + 状态显示

### 操作说明

| 操作 | 说明 |
|------|------|
| 车辆入库 | 点击按钮 → 抓拍 → 识别车牌 → 确认 → 写入数据库 |
| 车辆出库 | 点击按钮 → 抓拍 → 识别车牌 → 计算费用 → 确认 → 写入数据库 |
| RFID 刷卡 | 自动抓拍 → 识别 → 判断入库/出库 → 确认 |
| 查看记录 | 显示最近 20 条进出记录 |
| 锁屏 | 返回锁屏界面 |

### 数据存储

- **数据库路径**：`/frog/gate.db`
- **格式**：CSV 文本文件
- **内容**：车牌标识,操作类型(inbound/outbound),时间戳

### 重置车库数据

```bash
rm /frog/gate.db
```

删除后程序会自动创建新文件，车位计数归零。

## RFID 配置

### 硬件连接

- **模块**：HW-033 RFID 读卡器
- **串口**：`/dev/ttySAC1`（GEC6818 UART1）
- **波特率**：9600

### 卡号映射

在 `src/mainwindow.cpp` 构造函数中配置：

```cpp
// 初始化卡号-车牌映射（硬编码）
cardPlateMap_["83533443"] = "贵B91VIP";
// 添加更多卡号映射：
// cardPlateMap_["XXXXXXXX"] = "车牌号";
```

### 获取卡号

在开发板上刷卡测试，查看程序输出：

```
读到卡号: 83533443 (0x83533443)
```

## 故障排查

| 问题 | 解决方法 |
|------|----------|
| 摄像头黑屏 | `ls /dev/video*` 确认设备节点，代码默认 `/dev/video7` |
| 库找不到 | 确保 `cd` 到部署目录再运行，不要用绝对路径调用 |
| ALPR 启动失败 | 检查 `alpr/alpr` 存在且有执行权限 |
| ALPR 模型找不到 | 检查 `alpr/model/` 目录下有 9 个模型文件 |
| 识别结果为空 | 检查抓拍照片是否有清晰车牌 |
| RFID 无反应 | 检查串口连接，确认 `/dev/ttySAC1` 存在 |
| RFID 读取失败 | 检查串口权限：`chmod 666 /dev/ttySAC1` |
| 界面显示偏移 | 检查 LCD 分辨率是否为 800x480 |
| 超时不锁屏 | 检查是否有持续的触摸事件干扰 |
| 编译报 moc 错误 | 确认 `/usr/lib/x86_64-linux-gnu/qt5/bin/moc` 存在 |
| 编译报 Qt 头文件错误 | 确认 Qt-Embedded 路径正确 |

## 技术栈

| 组件 | 技术 |
|------|------|
| UI 框架 | Qt 5.7 Embedded |
| 图像处理 | OpenCV 3.4 |
| 车牌识别 | HyperLPR |
| RFID 通信 | 串口 UART (9600 8N1) |
| 构建系统 | CMake 3.10+ |
| 目标平台 | ARM Cortex-A53 (GEC6818) |

## 拓展功能

- [x] RFID 自动出入库
- [x] 超时自动锁屏（1分钟）
- [x] 开机动画
- [x] 车位计数显示
- [x] 进出记录查看
- [ ] GPIO 控制（蜂鸣器/LED）
- [ ] 远程监控
- [ ] 数据统计图表
