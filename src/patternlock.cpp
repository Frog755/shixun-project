#include "patternlock.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>
#include <QFont>

PatternLock::PatternLock(QWidget *parent)
    : QWidget(parent)
    , dotRadius_(0)       // 将在 resizeEvent 中根据屏幕尺寸计算
    , isDrawing_(false)
    , isSuccess_(true)
    , isCorrectPattern_(false)
    , lineColor_(30, 115, 230)
    , successColor_(15, 157, 88)
    , failColor_(219, 68, 55)
    , dotColor_(100, 100, 140)
    , selectedDotColor_(30, 115, 230)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void PatternLock::buildGrid()
{
    points_.clear();
    int cols = 3;
    int rows = 3;

    // 根据实际 widget 尺寸动态计算 dot 半径
    int minDim = qMin(width(), height());
    dotRadius_ = qMax(20, minDim / 10);

    // 计算正方形网格大小（取宽高中较小值的 60%）
    int gridSize = minDim * 60 / 100;

    // 计算间距
    int spacing = gridSize / (cols - 1);

    // 计算起始位置（居中）
    int startX = (width() - gridSize) / 2;
    int startY = (height() - gridSize) / 2;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int x = startX + c * spacing;
            int y = startY + r * spacing;
            points_.append(QPoint(x, y));
        }
    }
}

void PatternLock::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    buildGrid();
    update();
}

void PatternLock::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawGrid(painter);
    drawConnections(painter);
    drawDots(painter);
    if (isDrawing_) {
        drawCurrentLine(painter);
    }
}

void PatternLock::drawGrid(QPainter &painter)
{
    painter.fillRect(rect(), QColor(42, 42, 62));
}

void PatternLock::drawConnections(QPainter &painter)
{
    if (selected_.isEmpty()) return;

    QColor color = isCorrectPattern_ ? successColor_
         : (!isSuccess_ ? failColor_ : lineColor_);

    painter.setPen(QPen(color, qMax(3, dotRadius_ / 5), Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    for (int i = 0; i < (int)selected_.size() - 1; ++i) {
        QPoint p1 = points_.at(selected_.at(i));
        QPoint p2 = points_.at(selected_.at(i + 1));
        painter.drawLine(p1, p2);
    }
}

void PatternLock::drawDots(QPainter &painter)
{
    QFont font;
    font.setPointSize(qMax(8, dotRadius_));
    painter.setFont(font);

    for (int i = 0; i < (int)points_.size(); ++i) {
        QPoint p = points_.at(i);
        bool selected = selected_.contains(i);

        if (selected) {
            QColor color = isCorrectPattern_ ? successColor_
                 : (!isSuccess_ ? failColor_ : selectedDotColor_);
            painter.setBrush(color);
            painter.setPen(QPen(color.darker(120), 2));
            painter.drawEllipse(p, dotRadius_, dotRadius_);
            // 点内编号
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(p.x() - dotRadius_, p.y() + dotRadius_,
                           QString::number(i + 1));
        } else {
            painter.setBrush(dotColor_);
            painter.setPen(QPen(QColor(140, 140, 180), 2));
            painter.drawEllipse(p, dotRadius_, dotRadius_);
        }
    }
}

void PatternLock::drawCurrentLine(QPainter &painter)
{
    if (selected_.isEmpty()) return;

    QColor color = isCorrectPattern_ ? successColor_
         : (!isSuccess_ ? failColor_ : lineColor_);
    painter.setPen(QPen(color, qMax(3, dotRadius_ / 5), Qt::DashLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    QPoint lastPoint = points_.at(selected_.last());
    painter.drawLine(lastPoint, mousePos_);
}

int PatternLock::hitTest(const QPoint &pos) const
{
    // 判定范围比显示半径大，方便手指触摸
    int hitRadius = dotRadius_ + 15;
    for (int i = 0; i < (int)points_.size(); ++i) {
        double dx = points_.at(i).x() - pos.x();
        double dy = points_.at(i).y() - pos.y();
        if (std::sqrt(dx * dx + dy * dy) <= (double)hitRadius) {
            return i;
        }
    }
    return -1;
}

void PatternLock::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    int idx = hitTest(event->pos());
    if (idx >= 0) {
        isDrawing_ = true;
        isSuccess_ = true;
        isCorrectPattern_ = false;
        selected_.clear();
        selected_.append(idx);
        mousePos_ = event->pos();
        update();
    }
}

void PatternLock::mouseMoveEvent(QMouseEvent *event)
{
    if (!isDrawing_) return;

    mousePos_ = event->pos();
    int idx = hitTest(event->pos());
    if (idx >= 0 && !selected_.contains(idx)) {
        selected_.append(idx);
        update();
    }
}

void PatternLock::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!isDrawing_) return;

    isDrawing_ = false;
    handleRelease();
}

void PatternLock::handleRelease()
{
    if (selected_.size() < 4) {
        isSuccess_ = false;
        isCorrectPattern_ = false;
        update();
        emit unlockFailed();
        return;
    }

    isCorrectPattern_ = checkPattern();
    if (isCorrectPattern_) {
        update();
        emit unlockSuccess();
    } else {
        update();
        emit unlockFailed();
    }
}

void PatternLock::setCorrectPattern(const QVector<int> &pattern)
{
    correctPattern_ = pattern;
}

QVector<int> PatternLock::selectedPattern() const
{
    return selected_;
}

bool PatternLock::checkPattern()
{
    if (selected_.size() != correctPattern_.size()) {
        return false;
    }
    for (int i = 0; i < (int)correctPattern_.size(); ++i) {
        if (selected_.at(i) != correctPattern_.at(i)) {
            return false;
        }
    }
    return true;
}

void PatternLock::reset()
{
    isDrawing_ = false;
    isSuccess_ = true;
    isCorrectPattern_ = false;
    selected_.clear();
    update();
}
