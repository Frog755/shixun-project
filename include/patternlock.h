#ifndef PATTERNLOCK_H
#define PATTERNLOCK_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QColor>

/**
 * @brief 图形解锁组件（3x3 网格）
 * 参考文档：触摸屏坐标获取、点击判定
 * - 9 个点排列成 3x3 网格
 * - 触摸拖动绘制图案
 * - 至少连接 4 个点才能解锁
 */
class PatternLock : public QWidget
{
    Q_OBJECT

public:
    explicit PatternLock(QWidget *parent = nullptr);

    /**
     * @brief 设置正确密码
     */
    void setCorrectPattern(const QVector<int> &pattern);

    /**
     * @brief 获取当前绘制的密码
     */
    QVector<int> selectedPattern() const;

    /**
     * @brief 检查当前绘制是否正确
     */
    bool checkPattern();

    /**
     * @brief 重置状态
     */
    void reset();

signals:
    /**
     * @brief 解锁成功信号
     */
    void unlockSuccess();

    /**
     * @brief 解锁失败信号
     */
    void unlockFailed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void buildGrid();
    void drawGrid(QPainter &painter);
    void drawConnections(QPainter &painter);
    void drawDots(QPainter &painter);
    void drawCurrentLine(QPainter &painter);
    void handleRelease();
    int hitTest(const QPoint &pos) const;

    QVector<int> correctPattern_;   // 正确密码
    int dotRadius_;
    QVector<QPoint> points_;        // 网格点屏幕坐标
    QVector<int> selected_;         // 已选中的点索引
    QPoint mousePos_;               // 当前鼠标/触摸位置
    bool isDrawing_;                // 是否正在绘制
    bool isSuccess_;                // 绘制成功（点数>=4）
    bool isCorrectPattern_;         // 密码是否正确

    // 颜色
    QColor lineColor_;
    QColor successColor_;
    QColor failColor_;
    QColor dotColor_;
    QColor selectedDotColor_;
};

#endif // PATTERNLOCK_H