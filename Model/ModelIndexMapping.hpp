#ifndef MODEL_INDEX_MAPPING_HPP
#define MODEL_INDEX_MAPPING_HPP

#include <QAbstractProxyModel>

// Map a displayed item through any number of Qt proxy layers.
namespace ModelIndexMapping
{
inline QModelIndex toSource(QModelIndex index)
{
    while (index.isValid())
    {
        auto *proxy = qobject_cast<const QAbstractProxyModel *>(index.model());
        if (!proxy) break;
        index = proxy->mapToSource(index);
    }
    return index;
}

// Result actions need a row identity even when a calculated column is selected.
inline QModelIndex sourceRow(const QModelIndex &index)
{
    return toSource(index.siblingAtColumn(0));
}

inline QModelIndex fromSource(const QModelIndex &index, const QAbstractItemModel *viewModel)
{
    if (!index.isValid() || !viewModel) return {};
    if (index.model() == viewModel) return index;
    auto *proxy = qobject_cast<const QAbstractProxyModel *>(viewModel);
    if (!proxy) return {};
    return proxy->mapFromSource(fromSource(index, proxy->sourceModel()));
}
}
#endif
