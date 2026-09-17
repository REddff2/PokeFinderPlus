#pragma once
#include <QModelIndex>

struct ResultIdentity
{
    int species = 0;
    int form = -1;
    int gender = -1;
    bool shiny = false;
    int item = 0;
};
namespace SlotSprites
{
const QAbstractItemModel *rootModel(const QAbstractItemModel *model);
QModelIndex sourceIndex(QModelIndex index);
bool supported(const QAbstractItemModel *model);
bool spriteColumn(const QAbstractItemModel *model, int column);
ResultIdentity identity(const QModelIndex &index);
}
