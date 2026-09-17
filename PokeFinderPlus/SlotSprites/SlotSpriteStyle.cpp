#include "SlotSpriteStyle.hpp"
#include <QAbstractItemDelegate>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableView>
#include <typeinfo>

SlotSpriteStyle::SlotSpriteStyle(QStyle *ownedBase, SpriteResolver &resolver, SpriteCache &cache)
    : QProxyStyle(ownedBase), resolver(resolver), cache(cache) {}

bool SlotSpriteStyle::decorate(QStyleOptionViewItem &option, const QWidget *widget, bool painting) const
{
    const auto *table = qobject_cast<const QTableView *>(widget);
    if (!table || !option.index.isValid() || option.index.model() != table->model() || !option.icon.isNull()) return false;
    const auto *delegate = table->itemDelegateForIndex(option.index);
    if (!delegate) return false;
    // AnnotationDelegate has no Q_OBJECT; RTTI is needed to distinguish it.
    const QByteArray type(typeid(*delegate).name());
    if (typeid(*delegate) != typeid(QStyledItemDelegate) && type != "class AnnotationDelegate") return false;
    const auto identity = SlotSprites::identity(option.index);
    QSize bounds = identity.item ? QSize(24, 24) : QSize(34, 28);
    if (painting) bounds.scale(QSize(bounds.width(), qMax(1, option.rect.height() - 2)), Qt::KeepAspectRatio);
    const auto pixmap = cache.get(resolver.candidates(identity), bounds, widget->devicePixelRatioF());
    if (pixmap.isNull()) return false;
    QIcon icon;
    for (auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected})
        for (auto state : {QIcon::Off, QIcon::On}) icon.addPixmap(pixmap, mode, state);
    option.icon = icon;
    option.features |= QStyleOptionViewItem::HasDecoration;
    option.decorationSize = pixmap.deviceIndependentSize().toSize();
    option.decorationPosition = QStyleOptionViewItem::Left;
    option.decorationAlignment = Qt::AlignCenter;
    return true;
}
void SlotSpriteStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == CE_ItemViewItem)
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
        {
            auto decorated = *item;
            decorate(decorated, widget, true);
            QProxyStyle::drawControl(element, &decorated, painter, widget);
            return;
        }
    QProxyStyle::drawControl(element, option, painter, widget);
}
QSize SlotSpriteStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const
{
    if (type == CT_ItemViewItem)
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
        {
            auto decorated = *item;
            if (decorate(decorated, widget, false))
            {
                auto result = QProxyStyle::sizeFromContents(type, &decorated, size, widget);
                // Row geometry stays entirely with the host; short rows scale down at paint time.
                result.setHeight(QProxyStyle::sizeFromContents(type, option, size, widget).height());
                return result;
            }
        }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}
