#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QImage>
#include <QShowEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QMessageBox>
#include <opencv2/opencv.hpp>
#include <QMap>

// 命名管道路径
#define PIPE_INBOUND  "/tmp/alpr_inbound"
#define PIPE_OUTBOUND "/tmp/alpr_outbound"
// 照片保存路径
#define SNAP_PATH     "/tmp/vehicle_snap.jpg"
// alpr 可执行文件路径 - 用相对路径, 部署目录里 alpr 在 alpr/alpr
// 板子上不管当前目录是 deploy_patternlock_v2 还是 /opt, 都能找到
#define ALPR_BIN      "./alpr/alpr"
// 备选: 兼容老路径 (如果有人把 alpr 直接放在 /opt/)
// 也可以这样调用: /opt/alpr_deploy/alpr 等等

// 前向声明
class RfidReader;

/**
 * @brief 摄像头视频显示组件
 */
class CameraViewer : public QFrame
{
    Q_OBJECT
public:
    explicit CameraViewer(QWidget *parent = nullptr);
    ~CameraViewer();
    void startCamera(const QString &dev = "/dev/video7", int width = 640, int height = 480);
    void stopCamera();
    bool isCameraRunning() const;
    bool snapPhoto(const QString &path);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCaptureFrame();

private:
    void setupUI();
    QLabel *videoLabel_;
    cv::VideoCapture *cap_;
    cv::Mat frame_;
    bool running_;
    QTimer *timer_;
};

/**
 * @brief 主界面
 */
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void lockRequested();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onInboundClicked();
    void onOutboundClicked();
    void onLockClicked();
    void onAlprFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSoundFinished();
    void onCardDetected(const QString &cardId);  // RFID 卡检测到
    void onRfidError(const QString &errorMsg);   // RFID 错误
    void onIdleTimeout();  // 空闲超时锁屏
    void onViewRecordsClicked();  // 查看记录

private:
    void setupUI();
    void runALPR(const QString &photoPath, const QString &pipePath);
    void showPlateResult(const QString &plate, bool isInbound);
    QString queryLastAction(const QString &dbPath, const QString &plate);
    void saveToDatabase(const QString &dbPath, const QString &plate, bool isInbound);
    QString queryInboundTime(const QString &dbPath, const QString &plate);
    void showBill(const QString &plate, const QString &inTimeStr);
    void playSound(const QString &name);
    void startRfidReader();  // 启动 RFID 读取器
    void stopRfidReader();   // 停止 RFID 读取器
    QString lookupPlate(const QString &cardId);  // 根据卡号查找车牌
    int countVehiclesInPark();  // 统计当前库内车辆数
    void updateParkingCount();  // 更新车位计数显示
    void showRecentRecords();   // 显示最近记录

    CameraViewer *cameraViewer_;
    QPushButton *inboundBtn_;
    QPushButton *outboundBtn_;
    QPushButton *lockBtn_;
    QPushButton *viewRecordsBtn_;  // 查看记录按钮
    QLabel *statusLabel_;
    QLabel *rfidStatusLabel_;  // RFID 状态标签
    QLabel *parkingCountLabel_;  // 车位计数标签

    QProcess *alprProcess_;
    QProcess *soundProcess_;
    bool currentIsInbound_;
    QString currentPipePath_;
    int pipeReadFd_;  // 管道读端，先打开再启动 alpr 避免死锁
    QTimer *idleTimer_;  // 空闲超时定时器

    // RFID 相关
    RfidReader *rfidReader_;  // RFID 读取器线程
    QMap<QString, QString> cardPlateMap_;  // 卡号 -> 车牌映射
    QString rfidCurrentPlate_;  // 当前 RFID 识别的车牌号
};

#endif // MAINWINDOW_H
