#include "objectdetector.h"
#include <QQueue>
#include <cstring>
#include <algorithm>

QVector<Blob> ObjectDetector::detect(const uint16_t zones[64], uint16_t threshold_mm)
{
    /* Mark foreground: valid reading AND closer than threshold. */
    bool fg[64] = {};
    for (int i = 0; i < 64; i++)
        fg[i] = (zones[i] > 0 && zones[i] < threshold_mm);

    int label[64];
    std::fill(label, label + 64, -1);

    QVector<Blob> blobs;
    int next_id = 0;

    /* 4-connected flood fill to find connected components. */
    for (int start = 0; start < 64; start++) {
        if (!fg[start] || label[start] >= 0)
            continue;

        Blob b{};
        b.id      = next_id++;
        b.min_row = 8;  b.max_row = -1;
        b.min_col = 8;  b.max_col = -1;

        QQueue<int> q;
        q.enqueue(start);
        label[start] = b.id;

        float dist_sum = 0.0f, row_sum = 0.0f, col_sum = 0.0f;

        while (!q.isEmpty()) {
            int idx = q.dequeue();
            int r   = idx / 8;
            int c   = idx % 8;

            b.zone_count++;
            dist_sum += zones[idx];
            row_sum  += r;
            col_sum  += c;
            b.min_row = std::min(b.min_row, r);
            b.max_row = std::max(b.max_row, r);
            b.min_col = std::min(b.min_col, c);
            b.max_col = std::max(b.max_col, c);

            const int dr[] = {-1, 1,  0, 0};
            const int dc[] = { 0, 0, -1, 1};
            for (int n = 0; n < 4; n++) {
                int nr = r + dr[n];
                int nc = c + dc[n];
                if (nr < 0 || nr >= 8 || nc < 0 || nc >= 8)
                    continue;
                int nidx = nr * 8 + nc;
                if (fg[nidx] && label[nidx] < 0) {
                    label[nidx] = b.id;
                    q.enqueue(nidx);
                }
            }
        }

        b.avg_distance_mm = dist_sum / b.zone_count;
        b.centroid_row    = row_sum  / b.zone_count;
        b.centroid_col    = col_sum  / b.zone_count;

        int h = b.max_row - b.min_row + 1;
        int w = b.max_col - b.min_col + 1;
        b.aspect_ratio = (w > 0) ? static_cast<float>(h) / w : 1.0f;

        if (b.aspect_ratio > 1.5f)
            b.shape = "Tall";
        else if (b.aspect_ratio < 0.67f)
            b.shape = "Wide";
        else
            b.shape = "Round";

        blobs.append(b);
    }

    return blobs;
}
