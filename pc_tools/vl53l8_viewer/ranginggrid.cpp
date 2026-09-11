#include "ranginggrid.h"
#include <QPainter>
#include <QPen>

RangingGrid::RangingGrid(QWidget *parent) : QWidget(parent)
{
    m_zones.fill(0, 64);
    setMinimumSize(240, 240);
}

void RangingGrid::setFrame(const QVector<uint16_t> &zones)
{
    m_zones = zones;
    update();
}

void RangingGrid::setBlobs(const QVector<Blob> &blobs)
{
    m_blobs = blobs;
    update();
}

void RangingGrid::setMaxDisplayDistance(int mm)
{
    m_maxDist = mm;
    update();
}

/* HSV gradient: close → red (hue 0), far → blue (hue 240°). */
QColor RangingGrid::distanceColor(uint16_t mm) const
{
    if (mm == 0)
        return QColor(30, 30, 30);
    const float t = qBound(0.0f, static_cast<float>(mm) / m_maxDist, 1.0f);
    return QColor::fromHsvF(t * 0.667f, 0.85f, 0.95f);
}

void RangingGrid::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W = width();
    const int H = height();
    const int cw = W / 8;
    const int ch = H / 8;

    /* --- Heatmap cells --- */
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            const int idx  = row * 8 + col;
            const uint16_t d = (idx < m_zones.size()) ? m_zones[idx] : 0;
            const QRect cell(col * cw, row * ch, cw, ch);

            p.fillRect(cell, distanceColor(d));

            QFont f = p.font();
            f.setPointSize(7);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(cell, Qt::AlignCenter, d > 0 ? QString::number(d) : "----");
        }
    }

    /* --- Grid lines --- */
    p.setPen(QPen(QColor(70, 70, 70), 1));
    for (int i = 0; i <= 8; i++) {
        p.drawLine(i * cw, 0, i * cw, H);
        p.drawLine(0, i * ch, W, i * ch);
    }

    /* --- Blob bounding boxes and annotations --- */
    for (const Blob &b : std::as_const(m_blobs)) {
        const QRect bbox(
            b.min_col * cw,
            b.min_row * ch,
            (b.max_col - b.min_col + 1) * cw,
            (b.max_row - b.min_row + 1) * ch
        );

        p.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(bbox);

        /* Label: id + shape above the bounding box */
        QFont lf = p.font();
        lf.setPointSize(8);
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(Qt::yellow);
        p.drawText(bbox.topLeft() + QPoint(3, 13),
                   QString("#%1 %2").arg(b.id).arg(b.shape));

        /* Centroid crosshair */
        const int cx = static_cast<int>(b.centroid_col * cw + cw / 2.0f);
        const int cy = static_cast<int>(b.centroid_row * ch + ch / 2.0f);
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(cx - 6, cy, cx + 6, cy);
        p.drawLine(cx, cy - 6, cx, cy + 6);
    }
}
