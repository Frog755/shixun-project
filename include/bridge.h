#ifndef BRIDGE_H
#define BRIDGE_H

#include <QObject>
#include <QStackedWidget>

#include "lockscreen.h"

/**
 * @brief 桥接类：解决信号槽参数不匹配 + 界面切换联动
 */
class Bridge : public QObject
{
    Q_OBJECT
public:
    explicit Bridge(QStackedWidget *s, LockScreen *ls, QObject *parent = nullptr)
        : QObject(parent), stack_(s), lockScreen_(ls) {}

public slots:
    void showMainWindow()
    {
        if (stack_) stack_->setCurrentIndex(1);
    }
    void showLockScreen()
    {
        if (lockScreen_) lockScreen_->reset();
        if (stack_) stack_->setCurrentIndex(0);
    }

private:
    QStackedWidget *stack_;
    LockScreen *lockScreen_;
};

#endif // BRIDGE_H
