#include "IVTotalController.hpp"
#include "IVTotalProxyModel.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPointer>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTimer>
#include <algorithm>

namespace
{
QAbstractItemModel *rootModel(QAbstractItemModel *model)
{
    while (auto *proxy = qobject_cast<QAbstractProxyModel *>(model)) model = proxy->sourceModel();
    return model;
}
bool supported(const QByteArray &name)
{
    static const QSet<QByteArray> names = {
        "EggModel3", "GameCubeGeneratorModel", "GameCubeSearcherModel", "PokeSpotModel",
        "StaticGeneratorModel3", "StaticSearcherModel3", "WildGeneratorModel3", "WildSearcherModel3",
        "EggGeneratorModel4", "EggSearcherModel4", "EventGeneratorModel4", "EventSearcherModel4",
        "StaticGeneratorModel4", "StaticSearcherModel4", "WildGeneratorModel4", "WildSearcherModel4", "PokeRadarModel4",
        "AdjacentSeedsModel", "DreamRadarGeneratorModel5", "DreamRadarSearcherModel5",
        "EggGeneratorModel5", "EggSearcherModel5", "EventGeneratorModel5", "EventSearcherModel5",
        "HiddenGrottoGeneratorModel5", "HiddenGrottoSearcherModel5", "PickupGeneratorModel5",
        "StaticGeneratorModel5", "StaticSearcherModel5", "WildGeneratorModel5", "WildSearcherModel5",
        "EggModel8", "StaticModel8", "UndergroundModel", "WildModel8"
    };
    return names.contains(name);
}
QCheckBox *statsControl(QAbstractItemModel *model)
{
    // Bind to the original result table's Filter, also for Advance Finder views
    // that share that result model. Poke Radar parents models to their own tab.
    QObject *owner = model->parent();
    if (!owner) return nullptr;
    QString filterName;
    if (auto *table = qobject_cast<QTableView *>(owner))
        filterName = QStringLiteral("filter") + table->objectName().mid(QStringLiteral("tableView").size());
    for (QObject *scope = owner; scope; scope = scope->parent())
    {
        if (!filterName.isEmpty())
            if (auto *filter = scope->findChild<QObject *>(filterName))
                return filter->findChild<QCheckBox *>(QStringLiteral("checkBoxShowStats"));
        if (QByteArray(model->metaObject()->className()) == "PokeRadarModel4")
        {
            auto boxes = scope->findChildren<QCheckBox *>(QStringLiteral("checkBoxShowStats"));
            if (boxes.size() == 1) return boxes.front();
        }
    }
    return nullptr;
}
}

struct IVTotalController::Binding : QObject
{
    QPointer<QTableView> view;
    QPointer<QAbstractItemModel> original;
    QPointer<QItemSelectionModel> originalSelection;
    QPointer<QItemSelectionModel> installedSelection;
    IVTotalProxyModel *columns;
    QSortFilterProxyModel *sorter;
    QByteArray headerState;
    bool wasSorting;
    bool syncing = false;

    Binding(QTableView *table, QAbstractItemModel *root, QCheckBox *stats) :
        view(table), original(table->model()), originalSelection(table->selectionModel()),
        headerState(table->horizontalHeader()->saveState()), wasSorting(table->isSortingEnabled())
    {
        const QPointer<QCheckBox> control(stats);
        const bool alwaysIVs = QByteArray(root->metaObject()->className()) == "AdjacentSeedsModel";
        columns = new IVTotalProxyModel(original, root->metaObject()->className(),
                                       [control, alwaysIVs] { return alwaysIVs || (control && !control->isChecked()); }, this);
        sorter = new QSortFilterProxyModel(this);
        sorter->setSourceModel(columns);
        if (auto *prior = qobject_cast<QSortFilterProxyModel *>(original.data()))
        {
            sorter->setSortCaseSensitivity(prior->sortCaseSensitivity());
            sorter->setSortLocaleAware(prior->isSortLocaleAware());
        }
        const QPersistentModelIndex current = originalSelection->currentIndex();
        const auto selection = originalSelection->selection();
        table->setModel(sorter);
        installedSelection = table->selectionModel();
        table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        table->setSortingEnabled(true);
        // Keep existing host listeners on the original selection model alive.
        connect(installedSelection, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &now) {
            if (syncing || !originalSelection) return;
            syncing = true;
            QModelIndex mapped = columns->mapToSource(sorter->mapToSource(now));
            if (!mapped.isValid() && now.isValid()) mapped = columns->mapToSource(sorter->mapToSource(now.siblingAtColumn(0)));
            originalSelection->setCurrentIndex(mapped, QItemSelectionModel::NoUpdate);
            syncing = false;
        });
        connect(originalSelection, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &now) {
            if (syncing || !installedSelection) return;
            syncing = true;
            installedSelection->setCurrentIndex(sorter->mapFromSource(columns->mapFromSource(now)), QItemSelectionModel::NoUpdate);
            syncing = false;
        });
        connect(installedSelection, &QItemSelectionModel::selectionChanged, this, [this] {
            if (syncing || !originalSelection || !installedSelection) return;
            syncing = true;
            originalSelection->select(columns->mapSelectionToSource(sorter->mapSelectionToSource(installedSelection->selection())),
                                      QItemSelectionModel::ClearAndSelect);
            syncing = false;
        });
        installedSelection->select(sorter->mapSelectionFromSource(columns->mapSelectionFromSource(selection)), QItemSelectionModel::ClearAndSelect);
        installedSelection->setCurrentIndex(sorter->mapFromSource(columns->mapFromSource(current)), QItemSelectionModel::NoUpdate);
        if (stats) connect(stats, &QCheckBox::toggled, columns, &IVTotalProxyModel::refreshColumns);
    }
    ~Binding() override
    {
        syncing = true;
        if (view && view->model() == sorter)
        {
            view->setSortingEnabled(false);
            view->setModel(original);
            auto *temporarySelection = view->selectionModel();
            if (originalSelection && originalSelection->model() == original)
            {
                view->setSelectionModel(originalSelection);
                if (temporarySelection != originalSelection) delete temporarySelection;
            }
            view->horizontalHeader()->restoreState(headerState);
            view->setSortingEnabled(wasSorting);
        }
        // Qt intentionally does not delete selection models on setModel().
        if (installedSelection && (!view || view->selectionModel() != installedSelection)) delete installedSelection.data();
    }
};

IVTotalController::IVTotalController()
{
    scan();
    qApp->installEventFilter(this);
}
IVTotalController::~IVTotalController()
{
    qApp->removeEventFilter(this);
    bindings.clear();
}
void IVTotalController::queueScan()
{
    if (queued || scanning) return;
    queued = true;
    QTimer::singleShot(0, this, [this] { queued = false; scan(); });
}
bool IVTotalController::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::Show:
    case QEvent::ChildPolished:
        if (qobject_cast<QWidget *>(watched)) queueScan();
        break;
    case QEvent::Paint:
        // A view can replace its model without emitting a Qt modelChanged signal.
        if (auto *table = qobject_cast<QTableView *>(watched->parent()))
        {
            if (!observedModels.contains(table) || observedModels.value(table) != table->model()) queueScan();
        }
        break;
    default: break;
    }
    return false;
}
void IVTotalController::scan()
{
    if (scanning) return;
    scanning = true;
    std::erase_if(bindings, [](const auto &b) { return !b->view || !b->original || b->view->model() != b->sorter; });
    for (QWidget *widget : QApplication::allWidgets())
    {
        auto *table = qobject_cast<QTableView *>(widget);
        if (!table) continue;
        if (!observedModels.contains(table))
            connect(table, &QObject::destroyed, this, [this, table] { observedModels.remove(table); queueScan(); });
        observedModels.insert(table, table->model());
        if (!table->model() || !table->selectionModel()) continue;
        if (std::any_of(bindings.begin(), bindings.end(), [table](const auto &b) { return b->view == table; })) continue;
        auto *root = rootModel(table->model());
        if (!root || !supported(root->metaObject()->className())) continue;
        auto *stats = statsControl(root);
        // Fail closed when a future host rearrangement makes display mode unknown.
        if (!stats && QByteArray(root->metaObject()->className()) != "AdjacentSeedsModel") continue;
        bindings.push_back(std::make_unique<Binding>(table, root, stats));
        observedModels.insert(table, table->model());
    }
    scanning = false;
}
