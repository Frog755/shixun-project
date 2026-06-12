#!/bin/bash
# ============================================================================
#  GEC6818 Smart Gate 一键编译 + 部署脚本
#
#  用法:
#    ./build.sh                     # 完整流程：Qt应用 + alpr + 部署包
#    ./build.sh clean               # 清理所有构建产物
#    ./build.sh qt                  # 只编译 Qt 应用
#    ./build.sh alpr                # 只编译 alpr 车牌识别
#    ./build.sh deploy              # 只打包部署文件
#
#  链路: Windows (claude code) 写代码
#       -> VMware 共享目录 /mnt/hgfs/shixun/codex
#       -> 本脚本编译
#       -> 部署包在 /tmp/deploy_alpr/
#       -> 人工拷贝到开发板
# ============================================================================

set -e

# ---- 路径配置 ----
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
SRC_DIR="$PROJECT_DIR"
BUILD_QT_DIR="$PROJECT_DIR/build-arm"
HYPERLPR_ZIP="/mnt/hgfs/shixun/09_opencv/原资料/zeusees-HyperLPR-master源码包.zip"
# zip 内顶层目录是 HyperLPR/
HYPERLPR_SRC="$HOME/HyperLPR"
ALPR_SRC_DIR="$HYPERLPR_SRC/Prj-Linux/lpr"
ALPR_BUILD_DIR="$ALPR_SRC_DIR/build-arm"
DEPLOY_DIR="/tmp/deploy_alpr"
# 板子能直接看到的目录 (VMware 共享目录) — 傻瓜式部署
DEPLOY_BOARD_DIR="/mnt/hgfs/shixun/deploy_patternlock_v2"

CROSS_PREFIX="/usr/local/arm/5.4.0/usr/bin"
CROSS_CXX="$CROSS_PREFIX/arm-linux-g++"
CROSS_CC="$CROSS_PREFIX/arm-linux-gcc"
CROSS_LIB="/usr/local/arm/5.4.0/usr/lib"

QTE_PREFIX="/mnt/hgfs/shixun/Qt-Embedded-5.7.0"
OPENCV_ROOT="/mnt/hgfs/shixun/09_opencv"
OPENCV_INCLUDE="$OPENCV_ROOT/tools/opencv/include"
OPENCV_LIB="$OPENCV_ROOT/lib"

QT_TARGET="Gec6818SmartGate"
ALPR_TARGET="alpr"

# ---- 颜色 ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

print_step() { echo -e "${GREEN}▶${NC} $1"; }
print_warn() { echo -e "${YELLOW}⚠${NC}  $1"; }
print_err()  { echo -e "${RED}✗${NC}  $1"; }
print_info() { echo -e "${CYAN}ℹ${NC}  $1"; }

# ---- 前置检查 ----
check_deps() {
    print_step "检查编译环境..."
    local fail=0
    [ ! -x "$CROSS_CXX" ] && { print_err "交叉编译器不存在: $CROSS_CXX"; fail=1; }
    [ ! -d "$QTE_PREFIX/lib" ] && { print_err "Qt-Embedded 路径不存在: $QTE_PREFIX"; fail=1; }
    [ ! -d "$OPENCV_LIB" ] && { print_err "OpenCV 路径不存在: $OPENCV_LIB"; fail=1; }
    [ ! -x "/usr/lib/x86_64-linux-gnu/qt5/bin/moc" ] && { print_err "moc 不存在"; fail=1; }
    [ ! -f "$HYPERLPR_ZIP" ] && { print_err "HyperLPR 源码包不存在: $HYPERLPR_ZIP"; fail=1; }
    if [ "$fail" -eq 1 ]; then
        exit 1
    fi
    print_info "交叉编译器: $CROSS_CXX"
    print_info "Qt-Embedded: $QTE_PREFIX"
    print_info "OpenCV: $OPENCV_ROOT"
    print_info "HyperLPR: $HYPERLPR_ZIP"
}

# ---- 1. 解压 HyperLPR 源码 ----
prepare_hyperlpr() {
    if [ -d "$HYPERLPR_SRC" ] && [ -d "$ALPR_SRC_DIR" ]; then
        print_step "HyperLPR 源码已存在: $HYPERLPR_SRC"
    else
        print_step "解压 HyperLPR 源码..."
        cd "$HOME"
        if command -v unzip >/dev/null 2>&1; then
            unzip -q -o "$HYPERLPR_ZIP"
        else
            python3 -c "import zipfile; zipfile.ZipFile('$HYPERLPR_ZIP').extractall('$HOME')"
        fi
        if [ ! -d "$ALPR_SRC_DIR" ]; then
            print_err "解压后未找到 $ALPR_SRC_DIR"
            exit 1
        fi
        print_info "解压完成: $HYPERLPR_SRC"
    fi

    # 用我们改写的 main.cpp 替换
    print_step "替换 main.cpp 为管道版..."
    cp "$SRC_DIR/src/alpr_main.cpp" "$ALPR_SRC_DIR/main.cpp"
    print_info "已复制 alpr_main.cpp -> $ALPR_SRC_DIR/main.cpp"
}

# ---- 2. 编译 Qt 应用 ----
build_qt() {
    print_step "编译 Qt 应用 (Gec6818SmartGate)..."
    mkdir -p "$BUILD_QT_DIR"
    cd "$BUILD_QT_DIR"

    # 配置（仅首次或源码变化时）
    if [ ! -f "$BUILD_QT_DIR/CMakeCache.txt" ]; then
        print_info "运行 cmake 配置..."
        cmake "$PROJECT_DIR" \
            -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/toolchain-gec6818.cmake" \
            -DQTE_PREFIX="$QTE_PREFIX" \
            2>&1 | grep -E "(错误|error|--)" | head -20
    else
        print_info "复用现有 CMakeCache.txt"
    fi

    # 编译（关键：让编译器能找到 libmpfr.so.4）
    LD_LIBRARY_PATH="$CROSS_LIB" make -j"$(nproc)" 2>&1 | \
        grep -vE "^(arm-linux-g\+\+: WARNING:|/usr/local/arm)" | \
        tail -25

    if [ ! -f "$BUILD_QT_DIR/$QT_TARGET" ]; then
        print_err "Qt 应用编译失败"
        exit 1
    fi
    print_info "Qt 应用编译成功: $(ls -lh $BUILD_QT_DIR/$QT_TARGET | awk '{print $5}')"
}

# ---- 3. 编译 alpr ----
build_alpr() {
    print_step "编译 alpr 车牌识别..."
    prepare_hyperlpr

    # 修复 alpr 的 CMakeLists.txt（替换为 toolchain + 我们已知的路径）
    fix_alpr_cmake

    mkdir -p "$ALPR_BUILD_DIR"
    cd "$ALPR_BUILD_DIR"

    if [ ! -f "$ALPR_BUILD_DIR/CMakeCache.txt" ]; then
        print_info "运行 cmake 配置 alpr..."
        cmake "$ALPR_SRC_DIR" \
            -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/toolchain-gec6818.cmake" \
            2>&1 | grep -E "(错误|error|--)" | head -20
    fi

    LD_LIBRARY_PATH="$CROSS_LIB" make -j"$(nproc)" 2>&1 | \
        grep -vE "^(arm-linux-g\+\+: WARNING:|/usr/local/arm)" | \
        tail -20

    if [ ! -f "$ALPR_SRC_DIR/$ALPR_TARGET" ]; then
        print_err "alpr 编译失败 (期望路径: $ALPR_SRC_DIR/$ALPR_TARGET)"
        exit 1
    fi
    print_info "alpr 编译成功: $(ls -lh $ALPR_SRC_DIR/$ALPR_TARGET | awk '{print $5}')"
}

# 修复 alpr 的 CMakeLists.txt，强制使用我们的工具链和 OpenCV 路径
fix_alpr_cmake() {
    local cmake_file="$ALPR_SRC_DIR/CMakeLists.txt"
    if [ ! -f "$cmake_file" ]; then
        print_warn "未找到 $cmake_file, 跳过修复"
        return
    fi

    # 如果包含我们的标记，说明已经修过
    if grep -q "GEC6818-CUSTOM" "$cmake_file" 2>/dev/null; then
        print_info "CMakeLists.txt 已修复过"
        return
    fi

    print_step "修复 alpr 的 CMakeLists.txt (注入工具链和OpenCV路径)..."

    # 在文件开头插入我们的修复
    local pre_block='
# === GEC6818-CUSTOM: 自动注入的工具链配置 (由 build.sh 写入) ===
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER /usr/local/arm/5.4.0/usr/bin/arm-linux-gcc)
set(CMAKE_CXX_COMPILER /usr/local/arm/5.4.0/usr/bin/arm-linux-g++)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
include_directories(/usr/local/arm/5.4.0/usr/include)
include_directories(/usr/local/arm/5.4.0/usr/arm-none-linux-gnueabi/include)
include_directories(/mnt/hgfs/shixun/09_opencv/tools/opencv/include)
include_directories(/mnt/hgfs/shixun/09_opencv/tools/opencv/include/opencv)
link_directories(/mnt/hgfs/shixun/09_opencv/lib)
# === END GEC6818-CUSTOM ===
'

    # 用 awk 在第一行之前插入
    { echo "# GEC6818-CUSTOM-MARKER"; echo "$pre_block"; cat "$cmake_file"; } > "$cmake_file.tmp"
    mv "$cmake_file.tmp" "$cmake_file"
    print_info "已注入工具链 + OpenCV 路径"
}

# ---- 4. 打包部署文件 (傻瓜式: 直接平铺到开发板能看到的目录) ----
do_deploy() {
    print_step "部署到 $DEPLOY_BOARD_DIR ..."
    mkdir -p "$DEPLOY_BOARD_DIR/lib"
    mkdir -p "$DEPLOY_BOARD_DIR/alpr/model"

    # Qt 应用 -> 直接放顶层
    if [ -f "$BUILD_QT_DIR/$QT_TARGET" ]; then
        cp "$BUILD_QT_DIR/$QT_TARGET" "$DEPLOY_BOARD_DIR/"
        print_info "✓ $QT_TARGET"
    fi

    # alpr + model -> 放到 alpr/ 子目录
    if [ -f "$ALPR_SRC_DIR/$ALPR_TARGET" ]; then
        cp "$ALPR_SRC_DIR/$ALPR_TARGET" "$DEPLOY_BOARD_DIR/alpr/"
        print_info "✓ alpr/"
    fi
    if [ -d "$ALPR_SRC_DIR/model" ]; then
        cp -r "$ALPR_SRC_DIR/model/"* "$DEPLOY_BOARD_DIR/alpr/model/"
        print_info "✓ alpr/model/ ($(ls $DEPLOY_BOARD_DIR/alpr/model/ | wc -l) 个文件)"
    fi

    # OpenCV 运行时库 -> 放到 lib/ 子目录 (rpath $ORIGIN/lib 找到)
    for lib in libopencv_core.so.3.4 libopencv_imgproc.so.3.4 \
               libopencv_imgcodecs.so.3.4 libopencv_dnn.so.3.4 \
               libopencv_objdetect.so.3.4 libopencv_ml.so.3.4 \
               libopencv_videoio.so.3.4 libopencv_highgui.so.3.4; do
        if [ -f "$OPENCV_LIB/$lib" ]; then
            cp "$OPENCV_LIB/$lib" "$DEPLOY_BOARD_DIR/lib/"
        fi
    done
    # 建 .so -> .so.3.4 的软链 (板子上 dlopen 经常按 .so 找)
    # vmhgfs 不支持 symlink, 失败要容错
    cd "$DEPLOY_BOARD_DIR/lib"
    for f in libopencv_core libopencv_imgproc libopencv_imgcodecs \
             libopencv_dnn libopencv_objdetect libopencv_ml \
             libopencv_videoio libopencv_highgui; do
        if [ -e "${f}.so.3.4" ] && [ ! -e "${f}.so" ]; then
            ln -sf "${f}.so.3.4" "${f}.so" 2>/dev/null || \
            cp "${f}.so.3.4" "${f}.so" 2>/dev/null || true
        fi
    done
    cd - >/dev/null
    print_info "✓ lib/ ($(ls $DEPLOY_BOARD_DIR/lib/ | wc -l) 个文件)"

    # alpr 用相对路径 ./alpr/alpr 调用, 不再需要 alpr_deploy 副本
    # (vmhgfs 不支持 symlink, 旧方案是用 cp 副本; 新方案不需要)

    # (不写 HOW_TO_RUN.txt, 用户说部署目录越简单越好)

    echo ""
    print_step "部署完成 ✓"
    echo ""
    echo "  板子上只需要:"
    echo "    cd /mnt/hgfs/shixun/deploy_patternlock_v2"
    echo "    ./Gec6818SmartGate"
    echo ""
}

# ---- clean ----
do_clean() {
    print_step "清理构建产物..."
    rm -rf "$BUILD_QT_DIR"
    print_info "已删除 $BUILD_QT_DIR"

    if [ -d "$ALPR_BUILD_DIR" ]; then
        rm -rf "$ALPR_BUILD_DIR"
        print_info "已删除 $ALPR_BUILD_DIR"
    fi

    if [ -f "$ALPR_SRC_DIR/CMakeLists.txt" ] && grep -q "GEC6818-CUSTOM" "$ALPR_SRC_DIR/CMakeLists.txt"; then
        print_info "(alpr 的 CMakeLists.txt 已注入标记，下次 build_alpr 会自动重新注入)"
    fi
}

# ---- 入口 ----
check_deps

case "${1:-all}" in
    clean)
        do_clean
        ;;
    qt)
        build_qt
        ;;
    alpr)
        build_alpr
        ;;
    deploy)
        do_deploy
        ;;
    all)
        build_qt
        build_alpr
        do_deploy
        ;;
    *)
        echo "用法: $0 [all|qt|alpr|deploy|clean]"
        exit 1
        ;;
esac

echo ""
print_step "完成 ✓"
