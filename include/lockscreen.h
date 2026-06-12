#ifndef LOCKSCREEN_H
#define LOCKSCREEN_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include "patternlock.h"

/**
 * @brief 锁屏界面
 * 显示图形解锁组件，验证通过后切换到主界面
 */
class LockScreen : public QWidget
{
    Q_OBJECT

public:
    explicit LockScreen(QWidget *parent = nullptr);
    ~LockScreen();

    /**
     * @brief 设置正确密码
     */
    void setCorrectPattern(const QVector<int> &pattern);

    /**
     * @brief 重置锁屏界面到初始状态
     */
    void reset();

signals:
    /**
     * @brief 解锁成功信号
     */
    void unlocked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onUnlockSuccess();
    void onUnlockFailed();
    void onToggleHint();
    void onTimerReset();
    void emit_unlocked();

private:
    void setupUI();

    PatternLock *patternLock_;
    QLabel *title_;
    QLabel *hint_;
    QLabel *patternHint_;
    QLabel *statusLabel_;
    QPushButton *showHintBtn_;

    QVector<int> correctPattern_;
    bool hintVisible_;
};

#endif // LOCKSCREEN_H
