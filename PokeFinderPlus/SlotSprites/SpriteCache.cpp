#include "SpriteCache.hpp"
#include <QImageReader>
#include <QtMath>

QPixmap SpriteCache::get(const QStringList &paths, const QSize &bounds, qreal dpr)
{
    if (bounds.isEmpty()) return {};
    for (const auto &path : paths)
    {
        auto *source = originals.object(path);
        if (!source)
        {
            QImageReader reader(path);
            const QSize size = reader.size();
            auto *pixmap = new QPixmap;
            if (size.width() <= 128 && size.height() <= 128 && size.isValid())
            {
                const QImage image = reader.read().convertToFormat(QImage::Format_ARGB32);
                QRect visible;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x)
                        if (qAlpha(image.pixel(x,y))) visible |= QRect(x,y,1,1);
                // Ignore transparent source padding when fitting a small table icon.
                // Original files stay intact; aspect ratio and nearest-neighbor pixels remain intact.
                if (!visible.isEmpty()) *pixmap = QPixmap::fromImage(image.copy(visible));
            }
            originals.insert(path, pixmap, qMax(1, pixmap->width() * pixmap->height() * 4 / 1024));
            source = originals.object(path);
        }
        if (!source || source->isNull()) continue;
        const QSize pixels(qMax(1, qRound(bounds.width() * dpr)), qMax(1, qRound(bounds.height() * dpr)));
        const QString key = path + '|' + QString::number(pixels.width()) + 'x' + QString::number(pixels.height())
            + '|' + QString::number(dpr);
        if (const auto *cached = scaled.object(key)) return *cached;
        auto pixmap = source->scaled(pixels, Qt::KeepAspectRatio, Qt::FastTransformation);
        pixmap.setDevicePixelRatio(dpr);
        scaled.insert(key, new QPixmap(pixmap), qMax(1, pixmap.width() * pixmap.height() * 4 / 1024));
        return pixmap;
    }
    return {};
}
