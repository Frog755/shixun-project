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
    // 深色背景，全屏
    setStyleSheet(
        "QWidget { "
        "    background-color: #1a1a2e; "
        "}"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 标题
    title_ = new QLabel(" 图形解锁", this);
    title_->setStyleSheet(
        "QLabel { "
        "    font-size: 24px; "
        "    font-weight: bold; "
        "    color: #e0e0e0; "
        "    padding: 10px 0 5px 0; "
        "}"
    );
    title_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title_);

    // 提示文字
    hint_ = new QLabel("请绘制解锁图案（至少4个点）", this);
    hint_->setStyleSheet(
        "QLabel { "
        "    font-size: 13px; "
        "    color: #999999; "
        "    padding: 5px 0 10px 0; "
        "}"
    );
    hint_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(hint_);

    // 参考图案提示
    patternHint_ = new QLabel("参考图案: ", this);
    patternHint_->setStyleSheet(
        "QLabel { "
        "    font-size: 12px; "
        "    color: #777777; "
        "    padding: 0 0 5px 0; "
        "}"
    );
    patternHint_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(patternHint_);

    // 图案解锁组件容器
    QFrame *lockFrame = new QFrame(this);
    lockFrame->setStyleSheet(
        "QFrame { "
        "    background-color: #2a2a3e; "
        "    border-radius: 15px; "
        "    border: 1px solid #3a3a5e; "
        "}"
    );
    QVBoxLayout *frameLayout = new QVBoxLayout(lockFrame);
    frameLayout->setContentsMargins(10, 10, 10, 10);

    patternLock_ = new PatternLock(lockFrame);
    frameLayout->addWidget(patternLock_);

    mainLayout->addWidget(lockFrame, 1);
    mainLayout->addSpacing(5);

    // 状态提示
    statusLabel_ = new QLabel("", this);
    statusLabel_->setStyleSheet(
        "QLabel { "
        "    font-size: 14px; "
        "    font-weight: bold; "
        "    color: #db4437; "
        "    padding: 5px 0 5px 0; "
        "}"
    );
    statusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel_);

    // 显示/隐藏密码按钮 - 加大触摸区域
    showHintBtn_ = new QPushButton(" 显示/隐藏密码", this);
    showHintBtn_->setStyleSheet(
        "QPushButton { "
        "    font-size: 14px; "
        "    background-color: #3a3a5e; "
        "    color: #cccccc; "
        "    border: 2px solid #555577; "
        "    border-radius: 8px; "
        "    padding: 15px 20px; "
        "}"
        "QPushButton:pressed { "
        "    background-color: #2a2a3e; "
        "}"
    );
    // 连接 PatternLock 的解锁信号到处理槽
    QObject::connect(patternLock_, SIGNAL(unlockSuccess()), this, SLOT(onUnlockSuccess()));
    QObject::connect(patternLock_, SIGNAL(unlockFailed()), this, SLOT(onUnlockFailed()));
    // 显示/隐藏密码按钮
    QObject::connect(showHintBtn_, SIGNAL(clicked()), this, SLOT(onToggleHint()));
    mainLayout->addWidget(showHintBtn_, 0, Qt::AlignCenter);
    mainLayout->addSpacing(10);
}

void LockScreen::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (patternLock_) {
        patternLock_->setMinimumSize(qMin(width(), height()) / 2, qMin(width(), height()) / 2);
    }
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
        "    font-size: 14px; "
        "    font-weight: bold; "
        "    color: #0f9d58; "
        "    padding: 5px 0 5px 0; "
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
        "    font-size: 14px; "
        "    font-weight: bold; "
        "    color: #db4437; "
        "    padding: 5px 0 5px 0; "
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
