#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <memory>
#include <vector>
class QTableView;
class QAbstractItemModel;

class IVTotalController final : public QObject
{
public:
    IVTotalController();
    ~IVTotalController() override;
private:
    struct Binding;
    std::vector<std::unique_ptr<Binding>> bindings;
    QHash<QTableView *, QPointer<QAbstractItemModel>> observedModels;
    bool queued = false;
    bool scanning = false;
    void queueScan();
    void scan();
    bool eventFilter(QObject *watched, QEvent *event) override;
};
