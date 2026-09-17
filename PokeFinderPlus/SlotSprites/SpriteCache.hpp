#pragma once
#include <QCache>
#include <QPixmap>
#include <QStringList>

class SpriteCache
{
public:
    QPixmap get(const QStringList &paths, const QSize &bounds, qreal dpr);
    void clear() { originals.clear(); scaled.clear(); }
private:
    QCache<QString, QPixmap> originals{8192}; // KiB, includes bounded negative entries
    QCache<QString, QPixmap> scaled{8192};
};
