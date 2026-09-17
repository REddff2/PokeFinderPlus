#pragma once
#include <QAbstractProxyModel>
#include <QPersistentModelIndex>
#include <functional>

// A flat, row-preserving projection. Qt's sorting proxy owns row ordering.
class IVTotalProxyModel final : public QAbstractProxyModel
{
    Q_OBJECT
public:
    IVTotalProxyModel(QAbstractItemModel *source, QByteArray context, std::function<bool()> showingIVs, QObject *parent);
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &) const override { return {}; }
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QModelIndex mapToSource(const QModelIndex &index) const override;
    QModelIndex mapFromSource(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    void refreshColumns();
    int totalColumn() const { return firstIV < 0 ? -1 : firstIV + 6; }
private:
    QByteArray context;
    std::function<bool()> showingIVs;
    int firstIV = -1;
    int sourceColumns = 0;
    QModelIndexList layoutIndexes;
    QList<QPersistentModelIndex> layoutSources;
    int findIVs() const;
    void refreshSchema();
};
