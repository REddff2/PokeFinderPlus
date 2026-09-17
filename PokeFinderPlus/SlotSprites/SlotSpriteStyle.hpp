#pragma once
#include "SpriteResolver.hpp"
#include "SpriteCache.hpp"
#include <QProxyStyle>
#include <QStyleOptionViewItem>

// The existing delegate retains ownership of rendering/annotation decisions.
// This table-local hook decorates only its transient item style option.
class SlotSpriteStyle final : public QProxyStyle
{
public:
    SlotSpriteStyle(QStyle *ownedBase, SpriteResolver &resolver, SpriteCache &cache);
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget = nullptr) const override;
private:
    bool decorate(QStyleOptionViewItem &option, const QWidget *widget, bool painting) const;
    SpriteResolver &resolver;
    SpriteCache &cache;
};
