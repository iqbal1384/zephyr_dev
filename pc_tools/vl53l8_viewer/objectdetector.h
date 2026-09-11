#pragma once
#include <cstdint>
#include <QString>
#include <QVector>

struct Blob {
    int     id;
    int     zone_count;
    float   centroid_row;
    float   centroid_col;
    int     min_row, max_row;
    int     min_col, max_col;
    float   avg_distance_mm;
    float   aspect_ratio;   /* height / width of bounding box */
    QString shape;          /* "Round", "Tall", or "Wide" */
};

class ObjectDetector
{
public:
    /* zones[row*8+col], value 0 means no target.
     * threshold_mm: zones closer than this are considered foreground. */
    static QVector<Blob> detect(const uint16_t zones[64], uint16_t threshold_mm);
};
