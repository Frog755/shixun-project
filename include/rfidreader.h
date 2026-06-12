#ifndef RFIDREADER_H
#define RFIDREADER_H

#include <QThread>
#include <QString>

/**
 * @brief RFID 读取器线程类
 * 使用 HW-033 模块通过串口读取 RFID 卡号
 * 协议：2字节起始码 + 4字节卡号 + 2字节校验
 */
class RfidReader : public QThread
{
    Q_OBJECT

public:
    explicit RfidReader(const QString &serialPort = "/dev/ttySAC1", QObject *parent = nullptr);
    ~RfidReader();

    /**
     * @brief 设置串口设备路径
     */
    void setSerialPort(const QString &port);

    /**
     * @brief 获取当前串口设备路径
     */
    QString serialPort() const;

    /**
     * @brief 停止线程
     */
    void stop();

signals:
    /**
     * @brief 检测到 RFID 卡
     * @param cardId 卡号（4字节十六进制字符串，如 "A1B2C3D4"）
     */
    void cardDetected(const QString &cardId);

    /**
     * @brief 错误信号
     * @param errorMsg 错误信息
     */
    void errorOccurred(const QString &errorMsg);

protected:
    /**
     * @brief 线程主循环
     */
    void run() override;

private:
    /**
     * @brief 打开串口
     * @return 成功返回 true
     */
    bool openSerialPort();

    /**
     * @brief 关闭串口
     */
    void closeSerialPort();

    /**
     * @brief 配置串口参数
     * @return 成功返回 true
     */
    bool configureSerialPort();

    /**
     * @brief 计算 BCC 校验
     * @param buf 数据缓冲区
     * @return 校验值
     */
    unsigned char CalBCCS(unsigned char *buf);

    /**
     * @brief 发送请求命令，检测是否有卡
     * @return 0 成功，-1 失败
     */
    int PiccRequest();

    /**
     * @brief 发送防碰撞命令，获取卡号
     * @return 卡号，0 表示失败
     */
    unsigned int PiccAnticoll();

    /**
     * @brief 读取一张卡的 ID
     * @return 卡号，0 表示无卡
     */
    unsigned int readID();

    QString serialPort_;  // 串口设备路径
    int fd_;              // 串口文件描述符
    volatile bool running_;  // 线程运行标志
};

#endif // RFIDREADER_H
