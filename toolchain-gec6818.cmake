# GEC6818 开发板交叉编译工具链配置
# GEC6818 = Samsung S5P6818, Cortex-A53 (ARMv7 32位模式)
#
# 使用方法:
#   cd build-arm
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-gec6818.cmake
#   make -j4

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译器路径
set(TOOLCHAIN_DIR /usr/local/arm/5.4.0/usr)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_DIR}/bin/arm-linux-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-g++)

# sysroot (含 libc.so / libpthread.so 等运行时库)
# 注意: arm-linux-gcc 5.4.0 的 sysroot 在 TOOLCHAIN_DIR/arm-none-linux-gnueabi/sysroot
set(CMAKE_SYSROOT ${TOOLCHAIN_DIR}/arm-none-linux-gnueabi/sysroot)
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR}/arm-none-linux-gnueabi/sysroot)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
