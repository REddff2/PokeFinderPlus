#include "AnnotationDelegate.hpp"
#include <QWidget>
#include <QPainter>
#include <QPixmap>

namespace
{
QBrush tintedBrush(const QBrush &background, const QColor &tint, const QSize &size, qreal dpr)
{
    // No model background: alpha blends directly over the viewport's existing
    // base/alternating-row painting. Never substitute a guessed table color.
    if (background.style() == Qt::NoBrush) return QBrush(tint);
    const QSize logical = background.style() == Qt::SolidPattern ? QSize(1, 1) : size.expandedTo(QSize(1, 1));
    QPixmap layer((QSizeF(logical) * dpr).toSize());
    layer.setDevicePixelRatio(dpr);
    layer.fill(Qt::transparent);
    QPainter painter(&layer);
    painter.fillRect(QRect(QPoint(), logical), background);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.fillRect(QRect(QPoint(), logical), tint);
    painter.end();
    if (background.style() == Qt::SolidPattern) return QBrush(layer.toImage().pixelColor(0, 0));
    return QBrush(layer);
}
}

AnnotationDelegate::AnnotationDelegate(QAbstractItemDelegate *original, std::function<QColor(const QModelIndex &)> colorFor,
                                       QObject *parent) : QStyledItemDelegate(parent), original(original), colorFor(std::move(colorFor))
{
    connect(original, &QAbstractItemDelegate::commitData, this, &QAbstractItemDelegate::commitData);
    connect(original, &QAbstractItemDelegate::closeEditor, this, &QAbstractItemDelegate::closeEditor);
    connect(original, &QAbstractItemDelegate::sizeHintChanged, this, &QAbstractItemDelegate::sizeHintChanged);
}

void AnnotationDelegate::paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const
{
    const QColor tint = colorFor(i);
    if ((!tint.isValid() || tint.alpha() == 0) && original)
        original->paint(p, o, i);
    else
        QStyledItemDelegate::paint(p, o, i);
}

void AnnotationDelegate::initStyleOption(QStyleOptionViewItem *o, const QModelIndex &i) const
{
    QStyledItemDelegate::initStyleOption(o, i);
    const QColor color = colorFor(i);
    if (color.isValid() && color.alpha() != 0)
    {
        const qreal dpr = o->widget ? o->widget->devicePixelRatioF() : 1.0;
        o->backgroundBrush = tintedBrush(o->backgroundBrush, color, o->rect.size(), dpr);
        // Tint the real native selection background too. Text/icon colors,
        // opacity, font, selection state and focus rendering stay with Qt.
        for (auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled })
            o->palette.setBrush(group, QPalette::Highlight,
                                tintedBrush(o->palette.brush(group, QPalette::Highlight), color, o->rect.size(), dpr));
    }
}

QSize AnnotationDelegate::sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const
{ return original ? original->sizeHint(o, i) : QStyledItemDelegate::sizeHint(o, i); }
QWidget *AnnotationDelegate::createEditor(QWidget *p, const QStyleOptionViewItem &o, const QModelIndex &i) const
{
    QWidget *editor = original ? original->createEditor(p, o, i) : QStyledItemDelegate::createEditor(p, o, i);
    editors.removeIf([](const auto &item) { return item.isNull(); });
    if (editor) editors.append(editor);
    return editor;
}

void AnnotationDelegate::restoreEditorFilters(QAbstractItemDelegate *delegate)
{
    for (const auto &editor : editors)
    {
        if (editor)
        {
            editor->removeEventFilter(this);
            if (delegate) editor->installEventFilter(delegate);
        }
    }
}
void AnnotationDelegate::destroyEditor(QWidget *w, const QModelIndex &i) const
{ if (original) original->destroyEditor(w, i); else QStyledItemDelegate::destroyEditor(w, i); }
void AnnotationDelegate::setEditorData(QWidget *w, const QModelIndex &i) const
{ if (original) original->setEditorData(w, i); else QStyledItemDelegate::setEditorData(w, i); }
void AnnotationDelegate::setModelData(QWidget *w, QAbstractItemModel *m, const QModelIndex &i) const
{ if (original) original->setModelData(w, m, i); else QStyledItemDelegate::setModelData(w, m, i); }
void AnnotationDelegate::updateEditorGeometry(QWidget *w, const QStyleOptionViewItem &o, const QModelIndex &i) const
{ if (original) original->updateEditorGeometry(w, o, i); else QStyledItemDelegate::updateEditorGeometry(w, o, i); }
bool AnnotationDelegate::editorEvent(QEvent *e, QAbstractItemModel *m, const QStyleOptionViewItem &o, const QModelIndex &i)
{ return original ? original->editorEvent(e, m, o, i) : QStyledItemDelegate::editorEvent(e, m, o, i); }
bool AnnotationDelegate::helpEvent(QHelpEvent *e, QAbstractItemView *v, const QStyleOptionViewItem &o, const QModelIndex &i)
{ return original ? original->helpEvent(e, v, o, i) : QStyledItemDelegate::helpEvent(e, v, o, i); }
QList<int> AnnotationDelegate::paintingRoles() const
{ return original ? original->paintingRoles() : QStyledItemDelegate::paintingRoles(); }
bool AnnotationDelegate::eventFilter(QObject *w, QEvent *e)
{ return original ? static_cast<QObject *>(original.data())->eventFilter(w, e) : QStyledItemDelegate::eventFilter(w, e); }
