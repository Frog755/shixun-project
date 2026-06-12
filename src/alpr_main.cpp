/**
 * alpr_main.cpp - 车牌识别子程序 (管道版)
 *
 * 用法: alpr <图片路径> <管道路径>
 * 识别结果写入命名管道，格式: "车牌号\n" 或 "NONE\n"
 *
 * 编译: 见 CMakeLists.txt 中的 alpr 目标
 */

#include "include/Pipeline.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

using namespace std;

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "用法: %s <图片路径> <管道路径>\n", argv[0]);
        return 1;
    }

    const char *imagePath = argv[1];
    const char *pipePath  = argv[2];

    // 检查图片是否存在
    if (access(imagePath, F_OK) != 0) {
        fprintf(stderr, "图片不存在: %s\n", imagePath);
        return 1;
    }

    // 初始化 HyperLPR
    pr::PipelinePR prc(
        "model/cascade.xml",
        "model/HorizonalFinemapping.prototxt",
        "model/HorizonalFinemapping.caffemodel",
        "model/Segmentation.prototxt",
        "model/Segmentation.caffemodel",
        "model/CharacterRecognization.prototxt",
        "model/CharacterRecognization.caffemodel",
        "model/SegmenationFree-Inception.prototxt",
        "model/SegmenationFree-Inception.caffemodel"
    );

    // 读取图片
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        fprintf(stderr, "无法读取图片: %s\n", imagePath);
        // 写入 NONE 到管道
        int fd = open(pipePath, O_WRONLY);
        if (fd >= 0) {
            write(fd, "NONE\n", 5);
            close(fd);
        }
        return 1;
    }

    // 识别车牌
    string plate;
    try {
        vector<pr::PlateInfo> res = prc.RunPiplineAsImage(image, pr::SEGMENTATION_FREE_METHOD);
        for (auto &st : res) {
            string pn = st.getPlateName();
            if (pn.length() == 9) {
                plate = pn;
                printf("检测到车牌: %s, 确信率: %.2f\n", pn.c_str(), st.confidence);
                break;  // 取第一个有效结果
            }
        }
    } catch (...) {
        printf("未检测到车牌\n");
    }

    // 写入管道
    int fd = open(pipePath, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "打开管道失败: %s\n", pipePath);
        return 1;
    }

    if (plate.empty()) {
        write(fd, "NONE\n", 5);
        printf("写入管道: NONE\n");
    } else {
        write(fd, plate.c_str(), plate.length());
        write(fd, "\n", 1);
        printf("写入管道: %s\n", plate.c_str());
    }

    close(fd);
    return 0;
}
