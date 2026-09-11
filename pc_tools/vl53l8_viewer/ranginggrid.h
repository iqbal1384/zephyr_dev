#pragma once
#include <QWidget>
#include <QVector>
#include <cstdint>
#include "objectdetector.h"

class RangingGrid : public QWidget
{
    Q_OBJECT
public:
    explicit RangingGrid(QWidget *parent = nullptr);

    void setFrame(const QVector<uint16_t> &zones);
    void setBlobs(const QVector<Blob> &blobs);
    void setMaxDisplayDistance(int mm);

protected:
    void paintEvent(QPaintEvent *) override;
    QSize sizeHint() const override { return {480, 480}; }
    QSize minimumSizeHint() const override { return {240, 240}; }

private:
    QVector<uint16_t> m_zones;
    QVector<Blob>     m_blobs;
    int               m_maxDist = 3000;

    QColor distanceColor(uint16_t mm) const;
};
