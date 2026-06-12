#include "mainwindow.h"
#include "rfidreader.h"
#include <QResizeEvent>
#include <QDebug>
#include <QShowEvent>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// ============ CameraViewer ============

CameraViewer::CameraViewer(QWidget *parent)
    : QFrame(parent)
    , cap_(nullptr)
    , running_(false)
    , timer_(nullptr)
{
    setStyleSheet(
        "QFrame { "
        "    background-color: #000000; "
        "    border: 1px solid #d0d0d0; "
        "    border-radius: 12px; "
        "}"
    );
    setupUI();
}

CameraViewer::~CameraViewer()
{
    stopCamera();
}

void CameraViewer::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    videoLabel_ = new QLabel(this);
    videoLabel_->setAlignment(Qt::AlignCenter);
    videoLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    videoLabel_->setStyleSheet(
        "QLabel { "
        "    background-color: #1a1a1a; "
        "    color: #888888; "
        "    font-size: 16px; "
        "}"
    );
    layout->addWidget(videoLabel_);
}

void CameraViewer::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    qDebug() << "CameraViewer resize:" << event->size();
    if (videoLabel_ && !frame_.empty()) {
        QImage img(frame_.data, frame_.cols, frame_.rows,
                   frame_.step, QImage::Format_RGB888);
        QPixmap pm = QPixmap::fromImage(img).scaled(
            videoLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        videoLabel_->setPixmap(pm);
    }
}

void CameraViewer::startCamera(const QString &dev, int width, int height)
{
    if (running_) {
        qWarning() << "摄像头已经在运行";
        return;
    }
    running_ = true;
    qDebug() << "尝试打开摄像头:" << dev << "尺寸:" << width << "x" << height;

    cap_ = new cv::VideoCapture(dev.toStdString(), cv::CAP_V4L2);
    if (!cap_->isOpened()) {
        qWarning() << "无法打开摄像头:" << dev << " (CAP_V4L2)";
        delete cap_;
        cap_ = new cv::VideoCapture(dev.toStdString());
        if (!cap_->isOpened()) {
            qWarning() << "无法打开摄像头:" << dev << " (默认)";
            running_ = false;
            delete cap_; cap_ = nullptr;
            videoLabel_->setText("无法打开摄像头\n" + dev);
            return;
        }
    }
    cap_->set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap_->set(cv::CAP_PROP_FRAME_HEIGHT, height);

    int actualW = cap_->get(cv::CAP_PROP_FRAME_WIDTH);
    int actualH = cap_->get(cv::CAP_PROP_FRAME_HEIGHT);
    qDebug() << "实际打开尺寸:" << actualW << "x" << actualH;

    timer_ = new QTimer(this);
    QObject::connect(timer_, SIGNAL(timeout()), this, SLOT(onCaptureFrame()));
    timer_->start(50);
    qDebug() << "摄像头定时器启动, 间隔50ms";
}

void CameraViewer::stopCamera()
{
    running_ = false;
    if (timer_) { timer_->stop(); timer_->deleteLater(); timer_ = nullptr; }
    if (cap_) { cap_->release(); delete cap_; cap_ = nullptr; }
}

void CameraViewer::onCaptureFrame()
{
    if (!cap_ || !cap_->isOpened() || !running_) return;
    *cap_ >> frame_;
    if (frame_.empty()) return;

    cv::cvtColor(frame_, frame_, cv::COLOR_BGR2RGB);
    if (videoLabel_) {
        QSize labelSize = videoLabel_->size();
        QImage img(frame_.data, frame_.cols, frame_.rows,
                   frame_.step, QImage::Format_RGB888);
        if (labelSize.width() > 0 && labelSize.height() > 0) {
            QPixmap pm = QPixmap::fromImage(img).scaled(
                labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            videoLabel_->setPixmap(pm);
        } else {
            videoLabel_->setPixmap(QPixmap::fromImage(img));
        }
    }
}

bool CameraViewer::snapPhoto(const QString &path)
{
    if (frame_.empty()) {
        qWarning() << "抓拍失败: 没有可用帧";
        return false;
    }
    // frame_ 是 RGB 格式，保存时需要转回 BGR
    cv::Mat bgr;
    cv::cvtColor(frame_, bgr, cv::COLOR_RGB2BGR);
    bool ok = cv::imwrite(path.toStdString(), bgr);
    if (ok) {
        qDebug() << "照片已保存:" << path;
    } else {
        qWarning() << "照片保存失败:" << path;
    }
    return ok;
}

bool CameraViewer::isCameraRunning() const
{
    return running_;
}

// ============ MainWindow ============

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , cameraViewer_(nullptr)
    , alprProcess_(nullptr)
    , soundProcess_(nullptr)
    , currentIsInbound_(false)
    , pipeReadFd_(-1)
    , rfidReader_(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);

    // 初始化卡号-车牌映射（硬编码）
    cardPlateMap_["83533443"] = "贵B91VIP";

    setupUI();
    startRfidReader();
    updateParkingCount();  // 初始化车位计数

    // 初始化空闲超时定时器（1分钟）
    idleTimer_ = new QTimer(this);
    idleTimer_->setSingleShot(true);
    idleTimer_->setInterval(60000);  // 60秒
    QObject::connect(idleTimer_, SIGNAL(timeout()), this, SLOT(onIdleTimeout()));
    idleTimer_->start();
    qDebug() << "空闲超时定时器已启动 (60秒)";
}

MainWindow::~MainWindow()
{
    stopRfidReader();
    if (idleTimer_) {
        idleTimer_->stop();
    }

    if (alprProcess_) {
        alprProcess_->kill();
        alprProcess_->waitForFinished(1000);
        delete alprProcess_;
    }
    if (soundProcess_) {
        soundProcess_->kill();
        soundProcess_->waitForFinished(500);
        delete soundProcess_;
    }
}

void MainWindow::setupUI()
{
    setStyleSheet(
        "QWidget { "
        "    background-color: #f5f5f5; "
        "}"
    );

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // === 左侧：摄像头 ===
    cameraViewer_ = new CameraViewer(this);
    cameraViewer_->setStyleSheet(
        "QFrame { "
        "    background-color: #000000; "
        "    border: 1px solid #d0d0d0; "
        "    border-radius: 12px; "
        "}"
    );
    mainLayout->addWidget(cameraViewer_, 3);

    // === 右侧：按钮 ===
    QFrame *btnFrame = new QFrame(this);
    btnFrame->setStyleSheet(
        "QFrame { "
        "    background-color: #ffffff; "
        "    border-radius: 12px; "
        "    border: 1px solid #e0e0e0; "
        "}"
    );
    QVBoxLayout *btnLayout = new QVBoxLayout(btnFrame);
    btnLayout->setContentsMargins(6, 8, 6, 8);
    btnLayout->setSpacing(6);

    QLabel *titleLabel = new QLabel("操作菜单", btnFrame);
    titleLabel->setStyleSheet(
        "QLabel { "
        "    font-size: 18px; "
        "    font-weight: bold; "
        "    color: #333333; "
        "    padding: 2px 0 3px 0; "
        "}"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    btnLayout->addWidget(titleLabel);

    // 状态标签
    statusLabel_ = new QLabel("", btnFrame);
    statusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 12px; "
        "    color: #3498db; "
        "    padding: 3px; "
        "    background-color: #f0f8ff; "
        "    border-radius: 5px; "
        "}"
    );
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    btnLayout->addWidget(statusLabel_);

    // RFID 状态标签
    rfidStatusLabel_ = new QLabel("等待刷卡...", btnFrame);
    rfidStatusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 10px; "
        "    color: #888888; "
        "    padding: 3px; "
        "    background-color: #f8f8f8; "
        "    border: 1px solid #e0e0e0; "
        "    border-radius: 5px; "
        "}"
    );
    rfidStatusLabel_->setAlignment(Qt::AlignCenter);
    rfidStatusLabel_->setWordWrap(true);
    btnLayout->addWidget(rfidStatusLabel_);

    // 车位计数标签
    parkingCountLabel_ = new QLabel("当前库内：0 辆", btnFrame);
    parkingCountLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 14px; "
        "    font-weight: bold; "
        "    color: #27ae60; "
        "    padding: 6px; "
        "    background-color: #e8f5e9; "
        "    border: 1px solid #c8e6c9; "
        "    border-radius: 6px; "
        "}"
    );
    parkingCountLabel_->setAlignment(Qt::AlignCenter);
    btnLayout->addWidget(parkingCountLabel_);

    inboundBtn_ = new QPushButton("车辆入库", btnFrame);
    inboundBtn_->setStyleSheet(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #3498db; color: #ffffff; border: none; border-radius: 8px; padding: 8px 5px; }"
        "QPushButton:pressed { background-color: #2980b9; }"
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }"
    );
    QObject::connect(inboundBtn_, SIGNAL(clicked()), this, SLOT(onInboundClicked()));
    btnLayout->addWidget(inboundBtn_);

    outboundBtn_ = new QPushButton("车辆出库", btnFrame);
    outboundBtn_->setStyleSheet(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #3498db; color: #ffffff; border: none; border-radius: 8px; padding: 8px 5px; }"
        "QPushButton:pressed { background-color: #2980b9; }"
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }"
    );
    QObject::connect(outboundBtn_, SIGNAL(clicked()), this, SLOT(onOutboundClicked()));
    btnLayout->addWidget(outboundBtn_);

    btnLayout->addStretch(1);

    // 查看记录按钮
    viewRecordsBtn_ = new QPushButton("查看记录", btnFrame);
    viewRecordsBtn_->setStyleSheet(
        "QPushButton { font-size: 13px; font-weight: bold; background-color: #95a5a6; color: #ffffff; border: none; border-radius: 6px; padding: 6px 5px; }"
        "QPushButton:pressed { background-color: #7f8c8d; }"
    );
    QObject::connect(viewRecordsBtn_, SIGNAL(clicked()), this, SLOT(onViewRecordsClicked()));
    btnLayout->addWidget(viewRecordsBtn_);

    lockBtn_ = new QPushButton("锁屏", btnFrame);
    lockBtn_->setStyleSheet(
        "QPushButton { font-size: 15px; font-weight: bold; background-color: #e74c3c; color: #ffffff; border: none; border-radius: 8px; padding: 8px 5px; }"
        "QPushButton:pressed { background-color: #3a2560; }"
    );
    QObject::connect(lockBtn_, SIGNAL(clicked()), this, SLOT(onLockClicked()));
    btnLayout->addWidget(lockBtn_, 0, Qt::AlignBottom);

    mainLayout->addWidget(btnFrame, 1);
}

// ============ 抓拍 + ALPR 流程 ============

void MainWindow::onInboundClicked()
{
    qDebug() << "车辆入库 - 开始抓拍识别";
    idleTimer_->start();  // 重置空闲定时器
    playSound("1");
    currentIsInbound_ = true;
    currentPipePath_ = PIPE_INBOUND;
    rfidCurrentPlate_.clear();  // 清空 RFID 车牌，避免干扰

    // 1. 抓拍照片
    if (!cameraViewer_->snapPhoto(SNAP_PATH)) {
        statusLabel_->setText("抓拍失败，请检查摄像头");
        return;
    }
    statusLabel_->setText("照片已抓拍，正在识别车牌...");
    inboundBtn_->setEnabled(false);
    outboundBtn_->setEnabled(false);

    // 2. 创建命名管道 + 启动 alpr
    runALPR(SNAP_PATH, PIPE_INBOUND);
}

void MainWindow::onOutboundClicked()
{
    qDebug() << "车辆出库 - 开始抓拍识别";
    idleTimer_->start();  // 重置空闲定时器
    playSound("2");
    currentIsInbound_ = false;
    currentPipePath_ = PIPE_OUTBOUND;
    rfidCurrentPlate_.clear();  // 清空 RFID 车牌，避免干扰

    if (!cameraViewer_->snapPhoto(SNAP_PATH)) {
        statusLabel_->setText("抓拍失败，请检查摄像头");
        return;
    }
    statusLabel_->setText("照片已抓拍，正在识别车牌...");
    inboundBtn_->setEnabled(false);
    outboundBtn_->setEnabled(false);

    runALPR(SNAP_PATH, PIPE_OUTBOUND);
}

void MainWindow::runALPR(const QString &photoPath, const QString &pipePath)
{
    // 创建命名管道 (如果不存在)
    struct stat st;
    if (stat(pipePath.toStdString().c_str(), &st) != 0) {
        if (mkfifo(pipePath.toStdString().c_str(), 0666) != 0) {
            qWarning() << "创建管道失败:" << pipePath << strerror(errno);
            statusLabel_->setText("创建管道失败");
            inboundBtn_->setEnabled(true);
            outboundBtn_->setEnabled(true);
            return;
        }
        qDebug() << "创建管道:" << pipePath;
    }

    // 关键：先打开管道读端，再启动 alpr 写端，避免死锁
    // alpr 的 open(O_WRONLY) 会阻塞直到读端打开
    if (pipeReadFd_ >= 0) { ::close(pipeReadFd_); pipeReadFd_ = -1; }
    pipeReadFd_ = open(pipePath.toStdString().c_str(), O_RDONLY | O_NONBLOCK);
    if (pipeReadFd_ < 0) {
        qWarning() << "打开管道读端失败:" << pipePath << strerror(errno);
        statusLabel_->setText("管道打开失败");
        inboundBtn_->setEnabled(true);
        outboundBtn_->setEnabled(true);
        return;
    }
    qDebug() << "管道读端已打开, fd:" << pipeReadFd_;

    // 启动 alpr 子进程: alpr <photo_path> <pipe_path>
    if (alprProcess_) {
        alprProcess_->kill();
        alprProcess_->waitForFinished(1000);
        delete alprProcess_;
    }
    alprProcess_ = new QProcess(this);
    QObject::connect(alprProcess_, SIGNAL(finished(int, QProcess::ExitStatus)),
                     this, SLOT(onAlprFinished(int, QProcess::ExitStatus)));

    // 设置 LD_LIBRARY_PATH 让 alpr 能找到 OpenCV 库
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString appDir = QCoreApplication::applicationDirPath();
    QString libPath = appDir + "/lib";
    QString existingLdPath = env.value("LD_LIBRARY_PATH");
    if (!existingLdPath.isEmpty()) {
        libPath = libPath + ":" + existingLdPath;
    }
    env.insert("LD_LIBRARY_PATH", libPath);
    alprProcess_->setProcessEnvironment(env);

    // alpr 的 model 文件使用相对路径，需要设置工作目录为 ./alpr/
    alprProcess_->setWorkingDirectory(appDir + "/alpr");

    QString alprBin = appDir + "/alpr/alpr";
    qDebug() << "启动 alpr:" << alprBin << photoPath << pipePath;
    alprProcess_->start(alprBin, QStringList() << photoPath << pipePath);

    if (!alprProcess_->waitForStarted(3000)) {
        qWarning() << "alpr 启动失败, error:" << alprProcess_->errorString();
        statusLabel_->setText("车牌识别程序启动失败");
        inboundBtn_->setEnabled(true);
        outboundBtn_->setEnabled(true);
        ::close(pipeReadFd_); pipeReadFd_ = -1;
    }
}

void MainWindow::onAlprFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "alpr 进程结束, exitCode:" << exitCode << "exitStatus:" << exitStatus;

    // 从已打开的管道读端读取车牌
    QString plate;
    if (pipeReadFd_ >= 0) {
        // 用 select 等待数据，超时 15 秒
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(pipeReadFd_, &readfds);
        tv.tv_sec = 15;
        tv.tv_usec = 0;

        int ret = select(pipeReadFd_ + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0) {
            char buf[256] = {0};
            ssize_t n = read(pipeReadFd_, buf, sizeof(buf) - 1);
            if (n > 0) {
                plate = QString::fromUtf8(buf).trimmed();
                qDebug() << "从管道读取到:" << plate;
            }
        } else {
            qWarning() << "管道读取超时";
        }
        ::close(pipeReadFd_);
        pipeReadFd_ = -1;
    }

    inboundBtn_->setEnabled(true);
    outboundBtn_->setEnabled(true);

    if (plate.isEmpty() || plate == "NONE") {
        // ALPR 识别失败，但 RFID 流程中已有车牌号
        if (!rfidCurrentPlate_.isEmpty()) {
            qDebug() << "ALPR 识别失败，使用 RFID 映射车牌:" << rfidCurrentPlate_;
            showPlateResult(rfidCurrentPlate_, currentIsInbound_);
        } else {
            statusLabel_->setText("未识别到车牌，请重试");
        }
        return;
    }

    showPlateResult(plate, currentIsInbound_);
}


void MainWindow::showPlateResult(const QString &plate, bool isInbound)
{
    // 检查车牌号是否为空
    if (plate.isEmpty()) {
        statusLabel_->setText("车牌号为空，请重试");
        return;
    }

    QString action = isInbound ? "入库" : "出库";
    statusLabel_->setText(QString("识别结果: %1").arg(plate));

    // 业务校验：查询该车当前状态
    QString dbPath = "/frog/gate.db";
    QString lastAction = queryLastAction(dbPath, plate);

    QString errorMsg;
    if (isInbound) {
        if (lastAction == "inbound") {
            errorMsg = QString("车牌 %1 已在库内，不能重复入库").arg(plate);
        }
    } else {
        if (lastAction != "inbound") {
            errorMsg = QString("车牌 %1 不在库内，无法出库").arg(plate);
        }
    }

    if (!errorMsg.isEmpty()) {
        statusLabel_->setText(errorMsg);
        QMessageBox *errBox = new QMessageBox(this);
        errBox->setStyleSheet(
            "QMessageBox { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 12px; }"
            "QLabel { color: #e74c3c; font-size: 16px; }"
            "QPushButton { font-size: 14px; min-width: 80px; padding: 8px; "
            "    background-color: #e74c3c; color: #ffffff; border: none; "
            "    border-radius: 8px; }"
        );
        errBox->setWindowTitle("操作失败");
        errBox->setText(errorMsg);
        errBox->exec();
        return;
    }

    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setStyleSheet(
        "QMessageBox { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 12px; }"
        "QLabel { color: #333333; font-size: 16px; }"
        "QPushButton { font-size: 14px; min-width: 80px; padding: 8px; "
        "    background-color: #3498db; color: #ffffff; border: none; "
        "    border-radius: 8px; }"
    );
    msgBox->setWindowTitle(QString("车牌%1确认").arg(action));
    msgBox->setText(QString("识别车牌: %1\n\n确认%2?").arg(plate, action));
    msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox->button(QMessageBox::Yes)->setText("确认");
    msgBox->button(QMessageBox::No)->setText("取消");

    int ret = msgBox->exec();
    idleTimer_->start();  // 重置空闲定时器
    if (ret == QMessageBox::Yes) {
        if (!isInbound) {
            // 出库：查入库时间，算账单
            QString inTime = queryInboundTime(dbPath, plate);
            showBill(plate, inTime);
        }
        saveToDatabase(dbPath, plate, isInbound);
        updateParkingCount();  // 更新车位计数
        playSound(isInbound ? "3" : "4");
        statusLabel_->setText(QString("车牌 %1 已%2").arg(plate, action));
    } else {
        statusLabel_->setText("操作已取消");
    }
}

// 将车牌转为纯 ASCII key，中文用 Unicode 码点表示
// "贵A61000" → "U+8D35A61000", "京B12345" → "U+4EACB12345"
static QString plateToKey(const QString &plate)
{
    QString key;
    for (int i = 0; i < plate.length(); i++) {
        ushort code = plate[i].unicode();
        if (code > 127) {
            key += QString("U+%1").arg(code, 4, 16, QChar('0')).toUpper();
        } else {
            key += plate[i];
        }
    }
    return key;
}

// 将 Unicode 码点 key 转回车牌
// "U+8D35B91VIP" → "贵B91VIP", "U+4EACB12345" → "京B12345"
static QString keyToPlate(const QString &key)
{
    QString plate;
    int i = 0;
    while (i < key.length()) {
        if (i + 1 < key.length() && key[i] == 'U' && key[i + 1] == '+') {
            // 解析 Unicode 码点（固定4位十六进制）
            i += 2;  // 跳过 "U+"
            if (i + 4 <= key.length()) {
                QString hex = key.mid(i, 4);
                bool ok;
                ushort code = hex.toUShort(&ok, 16);
                if (ok) {
                    plate += QChar(code);
                    i += 4;
                } else {
                    plate += "U+";
                }
            } else {
                plate += "U+";
            }
        } else {
            plate += key[i];
            i++;
        }
    }
    return plate;
}

QString MainWindow::queryLastAction(const QString &dbPath, const QString &plate)
{
    QFile file(dbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "DB文件不存在:" << dbPath;
        return QString();
    }

    QString inputKey = plateToKey(plate);
    QString lastAction;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(",");
        if (parts.size() >= 2 && parts[0] == inputKey) {
            lastAction = parts[1];
        }
    }
    file.close();
    qDebug() << "查询" << plate << "(key:" << inputKey << ") 最后状态:" << lastAction;
    return lastAction;
}

void MainWindow::saveToDatabase(const QString &dbPath, const QString &plate, bool isInbound)
{
    QString action = isInbound ? "inbound" : "outbound";
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString key = plateToKey(plate);

    QFile file(dbPath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "数据库文件打开失败:" << dbPath;
        return;
    }

    QTextStream out(&file);
    out << key << "," << action << "," << timestamp << "\n";
    file.close();
    qDebug() << "已写入数据库:" << key << action << timestamp;
}

QString MainWindow::queryInboundTime(const QString &dbPath, const QString &plate)
{
    QFile file(dbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();

    QString inputKey = plateToKey(plate);
    QString inTime;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(",");
        if (parts.size() >= 3 && parts[0] == inputKey && parts[1] == "inbound") {
            inTime = parts[2];  // 最后一次入库时间
        }
    }
    file.close();
    qDebug() << "入库时间:" << inTime;
    return inTime;
}

void MainWindow::showBill(const QString &plate, const QString &inTimeStr)
{
    QDateTime inTime = QDateTime::fromString(inTimeStr, "yyyy-MM-dd HH:mm:ss");
    QDateTime outTime = QDateTime::currentDateTime();

    if (!inTime.isValid()) {
        QMessageBox::warning(this, "账单错误", "无法读取入库时间");
        return;
    }

    qint64 seconds = inTime.secsTo(outTime);
    if (seconds < 0) seconds = 0;

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    // 计费规则：每秒1元（演示用）
    int rate = 1;
    int totalFee = seconds * rate;

    QString billText = QString(
        "===== 停车账单 =====\n\n"
        "车牌: %1\n"
        "入库: %2\n"
        "出库: %3\n\n"
        "停车时长: %4时%5分%6秒\n"
        "计费: %7秒 x %8元\n\n"
        "应付金额: %9 元\n"
        "====================="
    ).arg(plate, inTimeStr, outTime.toString("yyyy-MM-dd HH:mm:ss"))
     .arg(hours).arg(minutes).arg(secs)
     .arg(seconds).arg(rate).arg(totalFee);

    QMessageBox *billBox = new QMessageBox(this);
    billBox->setStyleSheet(
        "QMessageBox { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 12px; }"
        "QLabel { color: #333333; font-size: 14px; font-family: monospace; }"
        "QPushButton { font-size: 14px; min-width: 80px; padding: 8px; "
        "    background-color: #3498db; color: #ffffff; border: none; "
        "    border-radius: 8px; }"
    );
    billBox->setWindowTitle("出库账单");
    billBox->setText(billText);
    billBox->exec();
}

void MainWindow::onLockClicked()
{
    qDebug() << "锁屏按钮点击";
    emit lockRequested();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (event->spontaneous()) return;
    if (cameraViewer_ && !cameraViewer_->isCameraRunning()) {
        qDebug() << "MainWindow 显示，启动摄像头";
        cameraViewer_->startCamera();
    }
    idleTimer_->start();  // 重置空闲定时器
}

void MainWindow::playSound(const QString &name)
{
    // 音频文件: /frog/<name>.mp3
    QString filePath = "/frog/" + name + ".mp3";
    if (!QFile::exists(filePath)) {
        qWarning() << "音频文件不存在:" << filePath;
        return;
    }

    // 停止当前正在播放的音频
    if (soundProcess_ && soundProcess_->state() != QProcess::NotRunning) {
        qDebug() << "停止上一段音频";
        soundProcess_->kill();
        soundProcess_->waitForFinished(200);
    }

    qDebug() << "播放音频:" << filePath;
    if (!soundProcess_) {
        soundProcess_ = new QProcess(this);
        QObject::connect(soundProcess_, SIGNAL(finished(int)), this, SLOT(onSoundFinished()));
    }
    soundProcess_->start("madplay", QStringList() << filePath);
}

void MainWindow::onSoundFinished()
{
    qDebug() << "音频播放结束";
}

// ============ RFID 相关方法 ============

void MainWindow::startRfidReader()
{
    // 创建 RFID 读取器，使用 UART1
    // TODO: 根据实际硬件连接修改串口设备路径
    rfidReader_ = new RfidReader("/dev/ttySAC1", this);

    // 连接信号槽
    QObject::connect(rfidReader_, SIGNAL(cardDetected(QString)),
                     this, SLOT(onCardDetected(QString)));
    QObject::connect(rfidReader_, SIGNAL(errorOccurred(QString)),
                     this, SLOT(onRfidError(QString)));

    // 启动线程
    rfidReader_->start();
    qDebug() << "RFID 读取器已启动";
}

void MainWindow::stopRfidReader()
{
    if (rfidReader_) {
        rfidReader_->stop();
        delete rfidReader_;
        rfidReader_ = nullptr;
        qDebug() << "RFID 读取器已停止";
    }
}

QString MainWindow::lookupPlate(const QString &cardId)
{
    // 在映射表中查找车牌号
    if (cardPlateMap_.contains(cardId)) {
        return cardPlateMap_[cardId];
    }
    return QString();
}

void MainWindow::onCardDetected(const QString &cardId)
{
    qDebug() << "检测到 RFID 卡:" << cardId;
    idleTimer_->start();  // 重置空闲定时器

    // 更新 UI 状态
    rfidStatusLabel_->setText(QString("已刷卡: %1").arg(cardId));
    rfidStatusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 12px; "
        "    color: #27ae60; "
        "    padding: 6px; "
        "    background-color: #e8f5e9; "
        "    border: 1px solid #c8e6c9; "
        "    border-radius: 8px; "
        "}"
    );

    // 查找对应的车牌号
    QString plate = lookupPlate(cardId);
    if (plate.isEmpty()) {
        statusLabel_->setText(QString("未知卡号: %1").arg(cardId));
        rfidStatusLabel_->setText(QString("未知卡: %1").arg(cardId));
        rfidStatusLabel_->setStyleSheet(
            "QLabel { "
            "    font-size: 12px; "
            "    color: #e74c3c; "
            "    padding: 6px; "
            "    background-color: #fde8e8; "
            "    border: 1px solid #f5c6cb; "
            "    border-radius: 8px; "
            "}"
        );
        return;
    }

    statusLabel_->setText(QString("识别到车牌: %1").arg(plate));

    // 保存当前 RFID 识别的车牌号，供 ALPR 失败时使用
    rfidCurrentPlate_ = plate;

    // 查询数据库判断入库还是出库
    QString dbPath = "/frog/gate.db";
    QString lastAction = queryLastAction(dbPath, plate);

    qDebug() << "========== RFID 自动判断 ==========";
    qDebug() << "车牌:" << plate;
    qDebug() << "数据库最后状态:" << lastAction;

    bool isInbound;
    if (lastAction.isEmpty() || lastAction == "outbound") {
        // 车辆不在库内，执行入库
        isInbound = true;
        qDebug() << ">>> 判断结果: 入库";
    } else {
        // 车辆在库内，执行出库
        isInbound = false;
        qDebug() << ">>> 判断结果: 出库";
    }
    qDebug() << "====================================";

    // 播放提示音
    playSound(isInbound ? "1" : "2");

    // 设置当前操作类型
    currentIsInbound_ = isInbound;
    currentPipePath_ = isInbound ? PIPE_INBOUND : PIPE_OUTBOUND;

    // 抓拍照片
    if (!cameraViewer_->snapPhoto(SNAP_PATH)) {
        statusLabel_->setText("抓拍失败，请检查摄像头");
        return;
    }

    statusLabel_->setText(QString("照片已抓拍，正在识别车牌: %1...").arg(plate));

    // 禁用按钮，防止重复操作
    inboundBtn_->setEnabled(false);
    outboundBtn_->setEnabled(false);

    // 启动 ALPR 识别
    runALPR(SNAP_PATH, currentPipePath_);
}

void MainWindow::onRfidError(const QString &errorMsg)
{
    qWarning() << "RFID 错误:" << errorMsg;
    rfidStatusLabel_->setText("RFID 错误: " + errorMsg);
    rfidStatusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 12px; "
        "    color: #e74c3c; "
        "    padding: 6px; "
        "    background-color: #fde8e8; "
        "    border: 1px solid #f5c6cb; "
        "    border-radius: 8px; "
        "}"
    );
}

// ============ 空闲超时锁屏 ============

void MainWindow::onIdleTimeout()
{
    qDebug() << "空闲超时，自动锁屏";
    emit lockRequested();
}

// ============ 车位计数 ============

int MainWindow::countVehiclesInPark()
{
    QString dbPath = "/frog/gate.db";
    QFile file(dbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    // 遍历数据库，统计每辆车的最后状态
    QMap<QString, QString> lastActions;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(",");
        if (parts.size() >= 2) {
            lastActions[parts[0]] = parts[1];
        }
    }
    file.close();

    // 统计状态为 "inbound" 的车辆数
    int count = 0;
    for (const QString &action : lastActions.values()) {
        if (action == "inbound") {
            count++;
        }
    }
    return count;
}

void MainWindow::updateParkingCount()
{
    int count = countVehiclesInPark();
    parkingCountLabel_->setText(QString("当前库内：%1 辆").arg(count));
    qDebug() << "车位计数更新:" << count;
}

// ============ 进出记录查看 ============

void MainWindow::onViewRecordsClicked()
{
    qDebug() << "查看记录按钮点击";
    showRecentRecords();
}

void MainWindow::showRecentRecords()
{
    QString dbPath = "/frog/gate.db";
    QFile file(dbPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::information(this, "提示", "暂无记录");
        return;
    }

    // 读取所有记录
    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    file.close();

    // 只显示最近 20 条
    int start = qMax(0, lines.size() - 20);
    QStringList recentLines;
    for (int i = start; i < lines.size(); i++) {
        recentLines.append(lines[i]);
    }

    // 格式化显示
    QString recordText;
    recordText += "===== 最近进出记录 =====\n\n";

    for (const QString &line : recentLines) {
        QStringList parts = line.split(",");
        if (parts.size() >= 3) {
            // 将 Unicode 码点转换回中文车牌
            QString key = parts[0];
            QString plate = keyToPlate(key);
            QString action = parts[1] == "inbound" ? "入库" : "出库";
            QString time = parts[2];

            recordText += QString("%1  %2  %3\n")
                .arg(plate.leftJustified(10))
                .arg(action)
                .arg(time);
        }
    }

    recordText += "\n========================";

    QMessageBox *recordBox = new QMessageBox(this);
    recordBox->setStyleSheet(
        "QMessageBox { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 12px; }"
        "QLabel { color: #333333; font-size: 12px; font-family: monospace; }"
        "QPushButton { font-size: 14px; min-width: 80px; padding: 8px; "
        "    background-color: #3498db; color: #ffffff; border: none; "
        "    border-radius: 8px; }"
    );
    recordBox->setWindowTitle("进出记录");
    recordBox->setText(recordText);
    recordBox->exec();
}
