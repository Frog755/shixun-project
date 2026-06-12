#include "lockscreen.h"
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QScreen>
#include <QApplication>

LockScreen::LockScreen(QWidget *parent)
    : QWidget(parent)
    , hintVisible_(false)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

LockScreen::~LockScreen()
{
}

void LockScreen::setupUI()
{
    // 浅色背景，全屏
    setStyleSheet(
        "QWidget { "
        "    background-color: #f5f5f5; "
        "}"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 10, 15, 10);
    mainLayout->setSpacing(6);

    // 标题
    title_ = new QLabel(" 图形解锁", this);
    title_->setStyleSheet(
        "QLabel { "
        "    font-size: 22px; "
        "    font-weight: bold; "
        "    color: #333333; "
        "    padding: 3px 0 2px 0; "
        "}"
    );
    title_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title_);

    // 提示文字
    hint_ = new QLabel("请绘制解锁图案（至少4个点）", this);
    hint_->setStyleSheet(
        "QLabel { "
        "    font-size: 12px; "
        "    color: #888888; "
        "    padding: 1px 0 3px 0; "
        "}"
    );
    hint_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(hint_);

    // 参考图案提示
    patternHint_ = new QLabel("参考图案: ", this);
    patternHint_->setStyleSheet(
        "QLabel { "
        "    font-size: 11px; "
        "    color: #aaaaaa; "
        "    padding: 0 0 2px 0; "
        "}"
    );
    patternHint_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(patternHint_);

    // 图案解锁组件容器
    QFrame *lockFrame = new QFrame(this);
    lockFrame->setStyleSheet(
        "QFrame { "
        "    background-color: #ffffff; "
        "    border-radius: 12px; "
        "    border: 1px solid #e0e0e0; "
        "}"
    );
    QVBoxLayout *frameLayout = new QVBoxLayout(lockFrame);
    frameLayout->setContentsMargins(8, 8, 8, 8);

    patternLock_ = new PatternLock(lockFrame);
    frameLayout->addWidget(patternLock_);

    mainLayout->addWidget(lockFrame, 1);
    mainLayout->addSpacing(3);

    // 状态提示
    statusLabel_ = new QLabel("", this);
    statusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 13px; "
        "    font-weight: bold; "
        "    color: #e74c3c; "
        "    padding: 3px 0 3px 0; "
        "}"
    );
    statusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel_);

    // 显示/隐藏密码按钮 - 加大触摸区域
    showHintBtn_ = new QPushButton(" 显示/隐藏密码", this);
    showHintBtn_->setStyleSheet(
        "QPushButton { "
        "    font-size: 12px; "
        "    background-color: #ffffff; "
        "    color: #666666; "
        "    border: 1px solid #d0d0d0; "
        "    border-radius: 6px; "
        "    padding: 8px 12px; "
        "}"
        "QPushButton:pressed { "
        "    background-color: #e8e8e8; "
        "}"
    );
    // 连接 PatternLock 的解锁信号到处理槽
    QObject::connect(patternLock_, SIGNAL(unlockSuccess()), this, SLOT(onUnlockSuccess()));
    QObject::connect(patternLock_, SIGNAL(unlockFailed()), this, SLOT(onUnlockFailed()));
    // 显示/隐藏密码按钮
    QObject::connect(showHintBtn_, SIGNAL(clicked()), this, SLOT(onToggleHint()));
    mainLayout->addWidget(showHintBtn_, 0, Qt::AlignCenter);
    mainLayout->addSpacing(3);
}

void LockScreen::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void LockScreen::setCorrectPattern(const QVector<int> &pattern)
{
    correctPattern_ = pattern;
    patternLock_->setCorrectPattern(pattern);
}

void LockScreen::reset()
{
    statusLabel_->clear();
    patternLock_->reset();
}

void LockScreen::onToggleHint()
{
    if (!hintVisible_) {
        QStringList patternStr;
        for (int p : correctPattern_) {
            patternStr.append(QString::number(p + 1));
        }
        patternHint_->setText("参考图案: " + patternStr.join(" -> "));
        hintVisible_ = true;
    } else {
        patternHint_->setText("参考图案: ");
        hintVisible_ = false;
    }
}

void LockScreen::onUnlockSuccess()
{
    statusLabel_->setText(" 解锁成功！");
    statusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 15px; "
        "    font-weight: bold; "
        "    color: #27ae60; "
        "    padding: 8px 0 8px 0; "
        "}"
    );

    // 600ms 后调用 emit_unlocked 槽，该槽会发射 unlocked() 信号
    QTimer::singleShot(600, this, &LockScreen::emit_unlocked);
}

void LockScreen::emit_unlocked()
{
    emit unlocked();
}

void LockScreen::onUnlockFailed()
{
    statusLabel_->setText(" 密码错误，请重试");
    statusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 15px; "
        "    font-weight: bold; "
        "    color: #e74c3c; "
        "    padding: 8px 0 8px 0; "
        "}"
    );

    // 直接调用 reset 方法，不用 lambda
    QTimer::singleShot(1000, this, &LockScreen::onTimerReset);
}

void LockScreen::onTimerReset()
{
    statusLabel_->clear();
    patternLock_->reset();
}
