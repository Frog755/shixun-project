# GEC6818 车牌识别门禁系统 - 部署指南

> **注意**：本项目的所有编译打包已自动化，请使用 `build.sh` 一键完成。
> 本文档仅供参考，如与 `build.sh` 行为冲突，以 `build.sh` 为准。

## 整体架构

```
摄像头抓拍 → 保存jpg → 启动alpr子进程 → 识别车牌 → 写入命名管道(FIFO)
    → Qt主程序读取管道 → 确认 → 写入SQLite数据库
```

- 入库管道: `/tmp/alpr_inbound`
- 出库管道: `/tmp/alpr_outbound`
- 临时照片: `/tmp/vehicle_snap.jpg`
- 数据库: `/opt/gate.db`

---

## 第一步: 编译 (在 Linux 虚拟机，一行搞定)

```bash
cd /mnt/hgfs/shixun/project
./build.sh
```

脚本自动完成：
1. 检查交叉编译工具链 / Qt-Embedded / OpenCV / HyperLPR 源码包
2. 编译 Qt 应用 `Gec6818SmartGate` (193KB)
3. 解压 HyperLPR 源码 + 替换为管道版 main.cpp + 修复 CMakeLists.txt + 编译 alpr (292KB)
4. 部署到 `/mnt/hgfs/shixun/deploy_patternlock_v2/`

支持的子命令：
- `./build.sh`            = `./build.sh all` （完整流程）
- `./build.sh qt`         （只编译 Qt 应用）
- `./build.sh alpr`       （只编译 alpr）
- `./build.sh deploy`     （只打包）
- `./build.sh clean`      （清理构建产物）

---

## 第二步: 拷贝到 U 盘

`build.sh` 产物在 `/mnt/hgfs/shixun/deploy_patternlock_v2/`。

U 盘插入 Ubuntu 主机后通常自动挂载到 `/media/$USER/<U盘名>/`。如果没有自动挂载：

```bash
# 查看 U 盘设备
lsblk

# 假设 U 盘是 /dev/sdb1
sudo mount /dev/sdb1 /mnt/usb
```

拷贝：

```bash
USB=/media/$USER/<U盘名>   # 或 /mnt/usb
cp -r /mnt/hgfs/shixun/deploy_patternlock_v2/* $USB/
sync
```

`deploy_patternlock_v2/` 内容：
```
Gec6818SmartGate     主程序
alpr/                车牌识别子程序
  ├── alpr
  └── model/         AI 模型
lib/                 OpenCV 动态库
```

---

## 第三步: 部署到开发板 (通过 U 盘)

```bash
# 1. 插 U 盘，挂载
mkdir -p /mnt/usb
mount /dev/sda1 /mnt/usb

# 2. 拷到板子本地
mkdir -p /opt
cp -r /mnt/usb/deploy_patternlock_v2/* /opt/

# 3. 加执行权限
chmod +x /opt/Gec6818SmartGate /opt/alpr/alpr

# 4. 卸载 U 盘
umount /mnt/usb
```

> 板子本地 ext4 支持 symlink，但本项目不依赖 symlink（rpath 用 $ORIGIN，相对路径）。

---

## 第四步: 运行

```bash
# 1. 确认摄像头
ls /dev/video*
# 应该看到 /dev/video7 之类

# 2. 运行主程序
cd /opt
./Gec6818SmartGate
```

**注意**：
- 不需要 `export LD_LIBRARY_PATH=...` （rpath 已写好，会自动找 `./lib/`）
- 不需要 `sh install.sh` （没这步骤了）
- 板子上没挂载 VMware 共享目录，所有文件必须在板子本地

---

## 解锁密码

锁屏界面绘制：`1 → 2 → 3 → 6 → 9`

---

## 使用流程

1. 启动后显示锁屏界面，绘制图案 `1→2→3→6→9` 解锁
2. 进入主界面，摄像头实时预览
3. 点击 **车辆入库**：
   - 自动抓拍当前画面
   - 启动 `./alpr/alpr` 识别车牌
   - 识别结果通过命名管道传回主程序
   - 弹窗显示车牌号，确认后写入 SQLite
4. 点击 **车辆出库**：流程同上
5. 点击 **锁屏** 回到锁屏界面

---

## 查看数据库记录

```bash
sqlite3 /opt/gate.db "SELECT * FROM vehicle_records;"
```

---

## 故障排查

| 问题 | 解决 |
|------|------|
| 摄像头黑屏 | `ls /dev/video*` 确认节点，代码默认 `/dev/video7` |
| 库找不到 | 必须 `cd /opt` 再运行，不要用绝对路径调用 |
| alpr 启动失败 | 检查 `/opt/alpr/alpr` 存在且有执行权限 |
| alpr 模型找不到 | 检查 `/opt/alpr/model/` 9 个文件都在 |
| 识别结果为空 | 检查抓拍的照片是否有清晰车牌 |
| 管道读取超时 | 检查 alpr 是否能独立运行：`/opt/alpr/alpr /tmp/test.jpg /tmp/test_pipe` |
| sqlite3 命令不存在 | 板子上需要 sqlite3：`opkg install sqlite3` 或交叉编译一个 |
| Gec6818SmartGate 段错误 | 检查 `/opt/lib/` 完整 (16 个文件) |

---

## 手动编译（仅当 build.sh 失败时参考）

如果 `build.sh` 出现无法自动修复的问题，可以手动一步步来：

### 编译 Qt 应用

```bash
cd /mnt/hgfs/shixun/project
mkdir -p build-arm && cd build-arm

# 注意: 不要用 CMAKE_PREFIX_PATH，要用 toolchain 文件
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-gec6818.cmake \
         -DQTE_PREFIX=/mnt/hgfs/shixun/Qt-Embedded-5.7.0

# libmpfr.so.4 必须在工具链自带的 lib 目录
LD_LIBRARY_PATH=/usr/local/arm/5.4.0/usr/lib make -j$(nproc)
```

### 编译 alpr

```bash
# 解压
cd ~
unzip "/mnt/hgfs/shixun/09_opencv/原资料/zeusees-HyperLPR-master源码包.zip"
cd HyperLPR/Prj-Linux/lpr   # 注意顶层目录是 HyperLPR/

# 替换 main.cpp
cp /mnt/hgfs/shixun/project/src/alpr_main.cpp ./main.cpp

# CMakeLists.txt 需要:
# - 注释掉 find_package(OpenCV) 和所有 add_executable(TEST_*)
# - 硬编码链接 /mnt/hgfs/shixun/09_opencv/lib/libopencv_*.so.3.4
# - 加 libopencv_highgui.so.3.4 (imshow 用到)
# - 加 rpath = $ORIGIN/../lib (因为 alpr 在 alpr/ 子目录)

# 编译
mkdir -p build && cd build
cmake ..
make -j$(nproc)
# 产物在 ../alpr
```

**强烈建议优先用 `./build.sh`**，上面的手动步骤只是 `build.sh` 失败时的 fallback。
