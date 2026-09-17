#ifndef POKEFINDERPLUS_ANNOTATIONDELEGATE_HPP
#define POKEFINDERPLUS_ANNOTATIONDELEGATE_HPP

#include <QPointer>
#include <QColor>
#include <QStyledItemDelegate>
#include <functional>

// Only wraps an exact QStyledItemDelegate; custom delegates are left untouched.
class AnnotationDelegate final : public QStyledItemDelegate
{
public:
    AnnotationDelegate(QAbstractItemDelegate *original, std::function<QColor(const QModelIndex &)> colorFor, QObject *parent);
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    void destroyEditor(QWidget *, const QModelIndex &) const override;
    void setEditorData(QWidget *, const QModelIndex &) const override;
    void setModelData(QWidget *, QAbstractItemModel *, const QModelIndex &) const override;
    void updateEditorGeometry(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    bool editorEvent(QEvent *, QAbstractItemModel *, const QStyleOptionViewItem &, const QModelIndex &) override;
    bool helpEvent(QHelpEvent *, QAbstractItemView *, const QStyleOptionViewItem &, const QModelIndex &) override;
    QList<int> paintingRoles() const override;
    void restoreEditorFilters(QAbstractItemDelegate *delegate);

protected:
    void initStyleOption(QStyleOptionViewItem *, const QModelIndex &) const override;
    bool eventFilter(QObject *, QEvent *) override;

private:
    QPointer<QAbstractItemDelegate> original;
    std::function<QColor(const QModelIndex &)> colorFor;
    mutable QList<QPointer<QWidget>> editors;
};
#endif
