#include "rfidreader.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>

#define DEV_RFID "/dev/ttySAC1"

RfidReader::RfidReader(const QString &serialPort, QObject *parent)
    : QThread(parent)
    , serialPort_(serialPort)
    , fd_(-1)
    , running_(false)
{
}

RfidReader::~RfidReader()
{
    stop();
    closeSerialPort();
}

void RfidReader::setSerialPort(const QString &port)
{
    serialPort_ = port;
}

QString RfidReader::serialPort() const
{
    return serialPort_;
}

void RfidReader::stop()
{
    running_ = false;
    wait();
}

bool RfidReader::openSerialPort()
{
    fd_ = open(serialPort_.toStdString().c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        qWarning() << "无法打开串口:" << serialPort_ << strerror(errno);
        return false;
    }
    qDebug() << "串口已打开:" << serialPort_ << "fd:" << fd_;
    return true;
}

void RfidReader::closeSerialPort()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        qDebug() << "串口已关闭";
    }
}

bool RfidReader::configureSerialPort()
{
    struct termios old_cfg, new_cfg;
    bzero(&old_cfg, sizeof(struct termios));
    bzero(&new_cfg, sizeof(struct termios));

    // 获取原有配置
    if (tcgetattr(fd_, &old_cfg) != 0) {
        qWarning() << "获取串口配置失败:" << strerror(errno);
        return false;
    }

    // 激活选项
    new_cfg.c_cflag |= CLOCAL | CREAD;
    cfmakeraw(&new_cfg);

    // 设置波特率：9600
    cfsetispeed(&new_cfg, B9600);
    cfsetospeed(&new_cfg, B9600);

    // 设置字符大小：8位
    new_cfg.c_cflag &= ~CSIZE;
    new_cfg.c_cflag |= CS8;

    // 设置奇偶校验：无
    new_cfg.c_cflag &= ~PARENB;

    // 设置停止位：1
    new_cfg.c_cflag &= ~CSTOPB;

    // 设置最小字符和等待时间
    new_cfg.c_cc[VTIME] = 0;
    new_cfg.c_cc[VMIN] = 1;

    // 清除串口缓冲
    tcflush(fd_, TCIFLUSH);

    // 激活配置
    if (tcsetattr(fd_, TCSANOW, &new_cfg) != 0) {
        qWarning() << "设置串口配置失败:" << strerror(errno);
        return false;
    }

    qDebug() << "串口配置完成: 9600 8N1";
    return true;
}

unsigned char RfidReader::CalBCCS(unsigned char *buf)
{
    int i;
    unsigned char BCC = 0;
    for (i = 0; i < buf[0] - 2; ++i) {
        BCC ^= buf[i];
    }
    return ~BCC;
}

int RfidReader::PiccRequest()
{
    unsigned char Wbuf[128], Rbuf[128];
    bzero(Wbuf, 128);
    bzero(Rbuf, 128);

    // 构建请求数据帧
    Wbuf[0] = 0x07;
    Wbuf[1] = 0x02;
    Wbuf[2] = 'A';  // 0x41
    Wbuf[3] = 0x01;
    Wbuf[4] = 0x52;
    Wbuf[5] = CalBCCS(Wbuf);
    Wbuf[6] = 0x03;

    // IO复用
    fd_set rdfd;
    FD_ZERO(&rdfd);
    FD_SET(fd_, &rdfd);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;  // 500ms 超时

    // 发送请求数据帧
    write(fd_, Wbuf, 7);

    int ret = select(fd_ + 1, &rdfd, NULL, NULL, &timeout);

    switch (ret) {
    case -1:
        qDebug() << "select 出错";
        return -1;
    case 0:
        // 超时，没有卡
        return -1;
    default:
        ret = read(fd_, Rbuf, 128);
        if (ret < 0) {
            qDebug() << "请求应答，读取失败";
            return -1;
        } else if (Rbuf[2] == 0x00) {
            // 请求成功，检测到卡
            qDebug() << "请求成功，检测到卡片";
            return 0;
        }
    }
    return -1;
}

unsigned int RfidReader::PiccAnticoll()
{
    unsigned char Wbuf[128], Rbuf[128];
    bzero(Wbuf, 128);
    bzero(Rbuf, 128);

    // 构建防碰撞数据帧
    Wbuf[0] = 0x08;
    Wbuf[1] = 0x02;
    Wbuf[2] = 'B';  // 0x42
    Wbuf[3] = 0x02;
    Wbuf[4] = 0x93;
    Wbuf[5] = 0x00;
    Wbuf[6] = CalBCCS(Wbuf);
    Wbuf[7] = 0x03;

    // IO复用
    fd_set rdfd;
    FD_ZERO(&rdfd);
    FD_SET(fd_, &rdfd);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;  // 500ms 超时

    // 发送防碰撞数据帧
    write(fd_, Wbuf, 8);

    int ret = select(fd_ + 1, &rdfd, NULL, NULL, &timeout);

    switch (ret) {
    case -1:
        qDebug() << "select 出错";
        return 0;
    case 0:
        qDebug() << "timeout 防碰撞超时";
        return 0;
    default:
        ret = read(fd_, Rbuf, 128);
        if (ret < 0) {
            qDebug() << "请求应答，读取失败";
            return 0;
        } else if (Rbuf[2] == 0x00) {
            // 成功获取卡号
            unsigned int ID = Rbuf[7] << 24 | Rbuf[6] << 16 | Rbuf[5] << 8 | Rbuf[4] << 0;
            return ID;
        }
    }
    return 0;
}

unsigned int RfidReader::readID()
{
    // 请求天线范围的卡
    if (PiccRequest() != 0) {
        return 0;
    }

    // 进行防碰撞处理，获取卡号
    unsigned int ID = PiccAnticoll();
    return ID;
}

void RfidReader::run()
{
    qDebug() << "RFID 读取线程启动";

    running_ = true;

    while (running_) {
        // 每次循环都打开串口（与示例代码保持一致）
        int fd = open(serialPort_.toStdString().c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) {
            qWarning() << "无法打开串口:" << serialPort_;
            msleep(1000);
            continue;
        }

        // 初始化串口
        struct termios old_cfg, new_cfg;
        bzero(&old_cfg, sizeof(struct termios));
        bzero(&new_cfg, sizeof(struct termios));

        tcgetattr(fd, &old_cfg);
        new_cfg.c_cflag |= CLOCAL | CREAD;
        cfmakeraw(&new_cfg);
        cfsetispeed(&new_cfg, B9600);
        cfsetospeed(&new_cfg, B9600);
        new_cfg.c_cflag &= ~CSIZE;
        new_cfg.c_cflag |= CS8;
        new_cfg.c_cflag &= ~PARENB;
        new_cfg.c_cflag &= ~CSTOPB;
        new_cfg.c_cc[VTIME] = 0;
        new_cfg.c_cc[VMIN] = 1;
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd, TCSANOW, &new_cfg);

        // 读取卡号
        unsigned int ID = 0;

        // 请求
        unsigned char Wbuf[128], Rbuf[128];
        bzero(Wbuf, 128);
        bzero(Rbuf, 128);

        Wbuf[0] = 0x07;
        Wbuf[1] = 0x02;
        Wbuf[2] = 'A';
        Wbuf[3] = 0x01;
        Wbuf[4] = 0x52;
        Wbuf[5] = CalBCCS(Wbuf);
        Wbuf[6] = 0x03;

        fd_set rdfd;
        FD_ZERO(&rdfd);
        FD_SET(fd, &rdfd);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        write(fd, Wbuf, 7);

        int ret = select(fd + 1, &rdfd, NULL, NULL, &timeout);

        if (ret > 0) {
            ret = read(fd, Rbuf, 128);
            if (ret > 0 && Rbuf[2] == 0x00) {
                // 请求成功，进行防碰撞
                bzero(Wbuf, 128);
                bzero(Rbuf, 128);

                Wbuf[0] = 0x08;
                Wbuf[1] = 0x02;
                Wbuf[2] = 'B';
                Wbuf[3] = 0x02;
                Wbuf[4] = 0x93;
                Wbuf[5] = 0x00;
                Wbuf[6] = CalBCCS(Wbuf);
                Wbuf[7] = 0x03;

                FD_ZERO(&rdfd);
                FD_SET(fd, &rdfd);
                timeout.tv_sec = 0;
                timeout.tv_usec = 500000;

                write(fd, Wbuf, 8);

                ret = select(fd + 1, &rdfd, NULL, NULL, &timeout);
                if (ret > 0) {
                    ret = read(fd, Rbuf, 128);
                    if (ret > 0 && Rbuf[2] == 0x00) {
                        ID = Rbuf[7] << 24 | Rbuf[6] << 16 | Rbuf[5] << 8 | Rbuf[4] << 0;
                    }
                }
            }
        }

        close(fd);

        if (ID != 0) {
            // 读到卡号，转换为十六进制字符串
            QString cardId = QString("%1").arg(ID, 8, 16, QChar('0')).toUpper();
            qDebug() << "读到卡号:" << cardId << "(0x" << QString::number(ID, 16) << ")";
            emit cardDetected(cardId);

            // 防止同一张卡重复触发
            msleep(2000);
        } else {
            // 没读到，稍等一下再试
            msleep(200);
        }
    }

    qDebug() << "RFID 读取线程结束";
}
