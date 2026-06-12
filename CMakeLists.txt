cmake_minimum_required(VERSION 3.10)
project(Gec6818SmartGate VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 检测是否在交叉编译
if(CMAKE_CXX_COMPILER MATCHES "arm-linux" OR CMAKE_CXX_COMPILER MATCHES "aarch64" OR CMAKE_CXX_COMPILER MATCHES "arm-none")
    message(STATUS "检测到交叉编译模式")
    set(CROSS_COMPILE ON)

    # 自动检测 Qt-Embedded 路径
    if(NOT DEFINED QTE_PREFIX)
        if(EXISTS "/mnt/hgfs/shixun/Qt-Embedded-5.7.0/lib/libQt5Core.so")
            set(QTE_PREFIX "/mnt/hgfs/shixun/Qt-Embedded-5.7.0")
        elseif(EXISTS "/opt/qte/lib/libQt5Core.so")
            set(QTE_PREFIX "/opt/qte")
        elseif(EXISTS "/usr/local/qt5arm/lib/libQt5Core.so")
            set(QTE_PREFIX "/usr/local/qt5arm")
        else()
            message(FATAL_ERROR "无法自动检测 Qt-Embedded 路径，请设置 QTE_PREFIX")
        endif()
    endif()

    message(STATUS "Qt-Embedded 路径: ${QTE_PREFIX}")
    set(QT_PREFIX "${QTE_PREFIX}")

    # 查找 Qt5 头文件
    execute_process(COMMAND pkg-config --cflags Qt5Core Qt5Gui Qt5Widgets
        OUTPUT_VARIABLE QT5_CFLAGS)
    string(REGEX REPLACE "^ +" "" QT5_CFLAGS "${QT5_CFLAGS}")
    string(REGEX REPLACE " +" ";" QT5_INCLUDES "${QT5_CFLAGS}")
    # 去除每个 include 前的 -I
    foreach(INC ${QT5_INCLUDES})
        string(REPLACE "-I" "" INC_CLEAN "${INC}")
        list(APPEND QT5_INCLUDES_CLEAN "${INC_CLEAN}")
    endforeach()
    set(QT5_INCLUDES "${QT5_INCLUDES_CLEAN}")

    message(STATUS "Qt5 includes: ${QT5_INCLUDES}")

    # 设置 moc
    set(QT5_MOC_EXECUTABLE "/usr/lib/x86_64-linux-gnu/qt5/bin/moc")

    # OpenCV 路径（交叉编译产物）
    set(OPENCV_ROOT "/mnt/hgfs/shixun/09_opencv")
    set(OPENCV_INCLUDE "${OPENCV_ROOT}/tools/opencv/include")
    set(OPENCV_LIB "${OPENCV_ROOT}/lib")

else()
    message(STATUS "使用系统 Qt5 (PC调试模式)")
    set(CROSS_COMPILE OFF)

    find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui)

    set(QT5_MOC_EXECUTABLE "/usr/lib/x86_64-linux-gnu/qt5/bin/moc")
    if(NOT EXISTS "${QT5_MOC_EXECUTABLE}")
        message(FATAL_ERROR "moc not found at ${QT5_MOC_EXECUTABLE}")
    endif()
endif()

if(NOT QT5_MOC_EXECUTABLE)
    message(FATAL_ERROR "moc not found")
endif()
message(STATUS "MOC: ${QT5_MOC_EXECUTABLE}")

# 包含项目头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 收集源文件
set(SOURCES
    src/main.cpp
    src/touchscreen.cpp
    src/patternlock.cpp
    src/lockscreen.cpp
    src/mainwindow.cpp
)

# 生成 moc 文件
set(MOC_HDRS
    ${CMAKE_SOURCE_DIR}/include/patternlock.h
    ${CMAKE_SOURCE_DIR}/include/lockscreen.h
    ${CMAKE_SOURCE_DIR}/include/mainwindow.h
    ${CMAKE_SOURCE_DIR}/include/bridge.h
)

set(MOC_FILES)
foreach(MOC_HDR ${MOC_HDRS})
    get_filename_component(MOC_NAME ${MOC_HDR} NAME_WE)
    set(MOC_OUT ${CMAKE_CURRENT_BINARY_DIR}/moc_${MOC_NAME}.cpp)
    add_custom_command(
        OUTPUT ${MOC_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${QT5_MOC_EXECUTABLE} ${MOC_HDR} -o ${MOC_OUT}
        DEPENDS ${MOC_HDR}
        COMMENT "MOC ${MOC_NAME}.h"
    )
    list(APPEND MOC_FILES ${MOC_OUT})
endforeach()

list(APPEND SOURCES ${MOC_FILES})

# 生成可执行文件
add_executable(Gec6818SmartGate ${SOURCES})

if(CROSS_COMPILE)
    # 交叉编译模式：手动链接 Qt
    include_directories(${QT5_INCLUDES})
    target_compile_options(Gec6818SmartGate PRIVATE -fPIC)
    target_link_libraries(Gec6818SmartGate
        ${QTE_PREFIX}/lib/libQt5Core.so
        ${QTE_PREFIX}/lib/libQt5Gui.so
        ${QTE_PREFIX}/lib/libQt5Widgets.so
    )

    # 链接 OpenCV
    target_include_directories(Gec6818SmartGate PRIVATE 
        /mnt/hgfs/shixun/09_opencv/tools/opencv/include 
        /mnt/hgfs/shixun/09_opencv/tools/opencv/include/opencv)
    target_link_libraries(Gec6818SmartGate
        /mnt/hgfs/shixun/09_opencv/lib/libopencv_core.so.3.4
        /mnt/hgfs/shixun/09_opencv/lib/libopencv_imgproc.so.3.4
        /mnt/hgfs/shixun/09_opencv/lib/libopencv_imgcodecs.so.3.4
        /mnt/hgfs/shixun/09_opencv/lib/libopencv_videoio.so.3.4
    )

    # 关键: 在链接时直接把 $ORIGIN 写进 ELF rpath
    # 板子上 ./lib/ 下有 OpenCV 库, 当前目录就是部署目录
    # $ORIGIN 在链接时是字面量, 运行时会展开为可执行文件所在目录
    # 用 set_target_properties + LINK_FLAGS 而不是 target_link_options
    # (后者需要 CMake 3.13+, 老版本不支持)
    # cmake 字符串里 $ 不会被替换, 直接写 -Wl,-rpath,$ORIGIN 传给链接器
    set(_RPATH_FLAGS "-Wl,-rpath,$ORIGIN -Wl,-rpath,$ORIGIN/lib -Wl,-rpath,${QTE_PREFIX}/lib")
    set_target_properties(Gec6818SmartGate PROPERTIES
        LINK_FLAGS "${_RPATH_FLAGS}"
    )
else()
    # PC 模式：使用 CMake Qt5 targets
    target_link_libraries(Gec6818SmartGate
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
    )
endif()

# 编译选项
target_compile_options(Gec6818SmartGate PRIVATE -Wall -Wextra)

# 安装配置
install(TARGETS Gec6818SmartGate DESTINATION bin)
