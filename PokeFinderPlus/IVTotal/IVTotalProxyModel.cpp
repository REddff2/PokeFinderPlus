#include "IVTotalProxyModel.hpp"
#include <QCoreApplication>

IVTotalProxyModel::IVTotalProxyModel(QAbstractItemModel *source, QByteArray context,
                                   std::function<bool()> showingIVs, QObject *parent) :
    QAbstractProxyModel(parent), context(std::move(context)), showingIVs(std::move(showingIVs))
{
    setSourceModel(source);
    refreshSchema();
    connect(source, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex &, int first, int last) { beginInsertRows({}, first, last); });
    connect(source, &QAbstractItemModel::rowsInserted, this, [this] { endInsertRows(); });
    connect(source, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex &, int first, int last) { beginRemoveRows({}, first, last); });
    connect(source, &QAbstractItemModel::rowsRemoved, this, [this] { endRemoveRows(); });
    connect(source, &QAbstractItemModel::rowsAboutToBeMoved, this,
            [this](const QModelIndex &, int first, int last, const QModelIndex &, int dest) {
                beginMoveRows({}, first, last, {}, dest);
            });
    connect(source, &QAbstractItemModel::rowsMoved, this, [this] { endMoveRows(); });
    connect(source, &QAbstractItemModel::modelAboutToBeReset, this, [this] { beginResetModel(); });
    connect(source, &QAbstractItemModel::modelReset, this, [this] { refreshSchema(); endResetModel(); });
    // These host models change their column schema using headerDataChanged as well.
    connect(source, &QAbstractItemModel::headerDataChanged, this, [this](Qt::Orientation o, int first, int last) {
        if (o == Qt::Horizontal) refreshColumns();
        else emit headerDataChanged(o, first, last);
    });
    connect(source, &QAbstractItemModel::columnsAboutToBeInserted, this, [this] { beginResetModel(); });
    connect(source, &QAbstractItemModel::columnsInserted, this, [this] { refreshSchema(); endResetModel(); });
    connect(source, &QAbstractItemModel::columnsAboutToBeRemoved, this, [this] { beginResetModel(); });
    connect(source, &QAbstractItemModel::columnsRemoved, this, [this] { refreshSchema(); endResetModel(); });
    connect(source, &QAbstractItemModel::columnsAboutToBeMoved, this, [this] { beginResetModel(); });
    connect(source, &QAbstractItemModel::columnsMoved, this, [this] { refreshSchema(); endResetModel(); });
    connect(source, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &top, const QModelIndex &bottom) {
        // Some host setters report an incomplete column range. Refresh the row,
        // including the derived total and dependent formatting roles.
        if (top.isValid() && bottom.isValid() && columnCount())
            emit dataChanged(index(top.row(), 0), index(bottom.row(), columnCount() - 1));
    });
    connect(source, &QAbstractItemModel::layoutAboutToBeChanged, this, [this] {
        emit layoutAboutToBeChanged();
        layoutIndexes = persistentIndexList();
        layoutSources.clear();
        for (const auto &i : layoutIndexes)
            layoutSources.append(i.column() == totalColumn() ? sourceModel()->index(i.row(), 0) : mapToSource(i));
    });
    connect(source, &QAbstractItemModel::layoutChanged, this, [this] {
        QModelIndexList updated;
        for (int n = 0; n < layoutIndexes.size(); ++n)
            updated.append(layoutSources[n].isValid() ? index(layoutSources[n].row(), layoutIndexes[n].column()) : QModelIndex());
        changePersistentIndexList(layoutIndexes, updated);
        layoutIndexes.clear(); layoutSources.clear();
        emit layoutChanged();
    });
    connect(source, &QObject::destroyed, this, [this] {
        beginResetModel(); firstIV = -1; sourceColumns = 0; endResetModel();
    });
}

int IVTotalProxyModel::findIVs() const
{
    if (!sourceModel() || !showingIVs()) return -1;
    const char *labels[] = { "HP", "Atk", "Def", "SpA", "SpD", "Spe" };
    for (int c = 0; c + 5 < sourceModel()->columnCount(); ++c)
    {
        bool match = true;
        for (int i = 0; i < 6; ++i)
            match &= sourceModel()->headerData(c + i, Qt::Horizontal).toString()
                == QCoreApplication::translate(context.constData(), labels[i]);
        if (match) return c;
    }
    return -1;
}
void IVTotalProxyModel::refreshSchema()
{
    sourceColumns = sourceModel() ? sourceModel()->columnCount() : 0;
    firstIV = findIVs();
}
void IVTotalProxyModel::refreshColumns()
{
    const int next = findIVs();
    const int columns = sourceModel() ? sourceModel()->columnCount() : 0;
    if (columns != sourceColumns || (firstIV >= 0 && next >= 0 && firstIV != next))
    {
        beginResetModel(); refreshSchema(); endResetModel();
    }
    else if (firstIV >= 0 && next < 0)
    {
        beginRemoveColumns({}, totalColumn(), totalColumn()); firstIV = -1; endRemoveColumns();
    }
    else if (firstIV < 0 && next >= 0)
    {
        beginInsertColumns({}, next + 6, next + 6); firstIV = next; endInsertColumns();
    }
    if (columnCount()) emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
}
int IVTotalProxyModel::rowCount(const QModelIndex &p) const { return !p.isValid() && sourceModel() ? sourceModel()->rowCount() : 0; }
int IVTotalProxyModel::columnCount(const QModelIndex &p) const { return p.isValid() || !sourceModel() ? 0 : sourceColumns + (firstIV >= 0); }
QModelIndex IVTotalProxyModel::index(int row, int col, const QModelIndex &p) const
{
    return !p.isValid() && row >= 0 && row < rowCount() && col >= 0 && col < columnCount() ? createIndex(row, col) : QModelIndex();
}
QModelIndex IVTotalProxyModel::mapToSource(const QModelIndex &i) const
{
    if (!i.isValid() || i.model() != this || !sourceModel() || i.column() == totalColumn()) return {};
    return sourceModel()->index(i.row(), i.column() - (firstIV >= 0 && i.column() > totalColumn()));
}
QModelIndex IVTotalProxyModel::mapFromSource(const QModelIndex &i) const
{
    if (!i.isValid() || i.model() != sourceModel()) return {};
    return index(i.row(), i.column() + (firstIV >= 0 && i.column() >= totalColumn()));
}
QVariant IVTotalProxyModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.model() != this || !sourceModel()) return {};
    if (i.column() != totalColumn()) return sourceModel()->data(mapToSource(i), role);
    if (role == Qt::DisplayRole)
    {
        if (!showingIVs()) return {};
        int total = 0;
        for (int c = firstIV; c < firstIV + 6; ++c)
        {
            bool ok = false;
            const int value = sourceModel()->data(sourceModel()->index(i.row(), c)).toInt(&ok);
            if (!ok || value < 0 || value > 31) return {};
            total += value;
        }
        return total;
    }
    // Preserve row formatting (including Adjacent Seeds' target-row role).
    if (role == Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::BackgroundRole || role == Qt::ForegroundRole || role == Qt::FontRole || role >= Qt::UserRole)
        return sourceModel()->data(sourceModel()->index(i.row(), firstIV), role);
    return {};
}
QVariant IVTotalProxyModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (!sourceModel()) return {};
    if (o == Qt::Horizontal && section == totalColumn())
        return role == Qt::DisplayRole ? tr("IV Total") : QVariant();
    return sourceModel()->headerData(section - (o == Qt::Horizontal && firstIV >= 0 && section > totalColumn()), o, role);
}
Qt::ItemFlags IVTotalProxyModel::flags(const QModelIndex &i) const
{
    if (!i.isValid() || i.model() != this || !sourceModel()) return Qt::NoItemFlags;
    return i.column() == totalColumn() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : sourceModel()->flags(mapToSource(i));
}
bool IVTotalProxyModel::setData(const QModelIndex &i, const QVariant &value, int role)
{
    const auto source = mapToSource(i);
    return source.isValid() && sourceModel()->setData(source, value, role);
}
