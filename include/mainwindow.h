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

private:
    void setupUI();
    void runALPR(const QString &photoPath, const QString &pipePath);
    void showPlateResult(const QString &plate, bool isInbound);
    QString queryLastAction(const QString &dbPath, const QString &plate);
    void saveToDatabase(const QString &dbPath, const QString &plate, bool isInbound);
    QString queryInboundTime(const QString &dbPath, const QString &plate);
    void showBill(const QString &plate, const QString &inTimeStr);
    void playSound(const QString &name);

    CameraViewer *cameraViewer_;
    QPushButton *inboundBtn_;
    QPushButton *outboundBtn_;
    QPushButton *lockBtn_;
    QLabel *statusLabel_;

    QProcess *alprProcess_;
    QProcess *soundProcess_;
    bool currentIsInbound_;
    QString currentPipePath_;
    int pipeReadFd_;  // 管道读端，先打开再启动 alpr 避免死锁
};

#endif // MAINWINDOW_H
