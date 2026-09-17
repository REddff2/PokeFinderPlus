#include "AnnotationController.hpp"
#include "AnnotationDelegate.hpp"

#include <QApplication>
#include <QEvent>
#include <QHash>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QSet>
#include <QTableView>
#include <algorithm>
#include <typeinfo>

struct AnnotationController::TableBinding : QObject
{
    QPointer<QTableView> view;
    QPointer<QAbstractItemModel> model;
    QPointer<QAbstractItemDelegate> original;
    AnnotationDelegate *delegate;
    // Qt updates these identities when the model/proxy reorders its items.
    // QPersistentModelIndex hashes its stable shared identity, not its row.
    QHash<QPersistentModelIndex, QColor> cells;
    QHash<QPersistentModelIndex, QColor> rows;

    explicit TableBinding(QTableView *view) : view(view), model(view->model()), original(view->itemDelegate())
    {
        delegate = new AnnotationDelegate(original, [this](const QModelIndex &index) {
            if (index.model() != model) return QColor();
            const auto cell = cells.constFind(QPersistentModelIndex(index));
            return cell != cells.cend() ? cell.value() : rows.value(QPersistentModelIndex(index.siblingAtColumn(0)));
        }, this);
        view->setItemDelegate(delegate);
        auto reset = [this] {
            cells.clear();
            rows.clear();
            if (this->view) this->view->viewport()->update();
        };
        auto refresh = [this] {
            auto invalid = [](QHash<QPersistentModelIndex, QColor>::iterator it) { return !it.key().isValid(); };
            cells.removeIf(invalid);
            rows.removeIf(invalid);
            if (this->view) this->view->viewport()->update();
        };
        auto columnsChanged = [this, refresh] {
            // If columns were inserted before column zero, keep each surviving
            // row identity anchored to its current canonical column.
            QHash<QPersistentModelIndex, QColor> anchored;
            for (auto it = rows.cbegin(); it != rows.cend(); ++it)
                if (it.key().isValid()) anchored.insert(it.key().sibling(it.key().row(), 0), it.value());
            rows.swap(anchored);
            refresh();
        };
        // Sorting/layout changes preserve marks. Removal drops invalid items;
        // a full reset/new search discards the old model identities entirely.
        connect(model, &QAbstractItemModel::modelReset, this, reset);
        connect(model, &QObject::destroyed, this, reset);
        connect(model, &QAbstractItemModel::layoutChanged, this, refresh);
        connect(model, &QAbstractItemModel::rowsInserted, this, refresh);
        connect(model, &QAbstractItemModel::rowsRemoved, this, refresh);
        connect(model, &QAbstractItemModel::rowsMoved, this, refresh);
        connect(model, &QAbstractItemModel::columnsInserted, this, columnsChanged);
        connect(model, &QAbstractItemModel::columnsRemoved, this, columnsChanged);
        connect(model, &QAbstractItemModel::columnsMoved, this, columnsChanged);
    }

    ~TableBinding() override
    {
        if (view)
        {
            if (view->itemDelegate() == delegate)
                view->setItemDelegate(original ? original.data() : new QStyledItemDelegate(view));
            delegate->restoreEditorFilters(view->itemDelegate());
            view->viewport()->update();
        }
    }
};

AnnotationController::AnnotationController(QWidget *window) : QObject(window), window(window)
{
    start();
}

AnnotationController::~AnnotationController() { stop(); }
void AnnotationController::mark() { apply(true); }
void AnnotationController::clear() { apply(false); }
void AnnotationController::setRowMode(bool enabled) { rowMode = enabled; }
void AnnotationController::setColor(const QColor &value) { color = value; }

void AnnotationController::start()
{
    if (tracking) return;
    targetTable();
    tracking = true;
    focusConnection = connect(qApp, &QApplication::focusChanged, this, [this](QWidget *old, QWidget *now) {
        remember(old);
        remember(now);
    });
    qApp->installEventFilter(this);
}

QTableView *AnnotationController::tableForWidget(QWidget *widget) const
{
    if (!widget || widget == window || window->isAncestorOf(widget)) return nullptr;
    for (QWidget *parent = widget; parent; parent = parent->parentWidget())
    {
        if (auto *table = qobject_cast<QTableView *>(parent))
        {
            if (table->isVisible() && table->isEnabled() && table->model() && table->selectionModel()
                && table->window()->windowType() != Qt::Popup)
                return table;
            return nullptr;
        }
    }
    return nullptr;
}

void AnnotationController::remember(QWidget *widget)
{
    if (auto *table = tableForWidget(widget)) lastTable = table;
}

QTableView *AnnotationController::targetTable()
{
    remember(QApplication::focusWidget());
    if (lastTable && tableForWidget(lastTable)) return lastTable;
    lastTable = nullptr;
    if (tracking) return nullptr;
    // On first opening, the Plugins menu may already have taken focus. Use a
    // window's retained table focus only when there is one unambiguous selected
    // table. Never choose among multiple tables based on position or scan order.
    QTableView *candidate = nullptr;
    for (QWidget *top : QApplication::topLevelWidgets())
    {
        if (!top->isVisible() || top == window) continue;
        auto *table = tableForWidget(top->focusWidget());
        if (!table || !table->selectionModel()->hasSelection()) continue;
        if (candidate && candidate != table) return nullptr;
        candidate = table;
    }
    lastTable = candidate;
    return candidate;
}

void AnnotationController::stop()
{
    if (tracking)
    {
        qApp->removeEventFilter(this);
        disconnect(focusConnection);
        tracking = false;
    }
    lastTable = nullptr;
    tables.clear();
}

void AnnotationController::apply(bool marking)
{
    auto *view = targetTable();
    if (!view)
    {
        emit statusChanged(tr("No result cells selected"));
        return;
    }
    const auto selected = view->selectionModel()->selectedIndexes();
    QSet<QPersistentModelIndex> selectedCells;
    QSet<QPersistentModelIndex> selectedRows;
    for (const QModelIndex &index : selected)
    {
        if (index.isValid() && !index.parent().isValid())
        {
            selectedCells.insert(QPersistentModelIndex(index));
            selectedRows.insert(QPersistentModelIndex(index.siblingAtColumn(0)));
        }
    }
    if (selectedCells.isEmpty())
    {
        emit statusChanged(tr("No result cells selected"));
        return;
    }
    // Never partially mark a requested selection through custom per-row/column
    // delegates. Leave those delegates and their rendering untouched.
    if (marking)
    {
        for (const QModelIndex &index : selected)
        {
            if (view->itemDelegateForIndex(index) != view->itemDelegate())
            {
                emit statusChanged(tr("Custom table delegates are not supported yet"));
                return;
            }
        }
        if (rowMode)
        {
            for (const auto &row : selectedRows)
                for (int column = 0; column < view->model()->columnCount(); ++column)
                    if (view->itemDelegateForIndex(view->model()->index(row.row(), column)) != view->itemDelegate())
                    {
                        emit statusChanged(tr("Custom table delegates are not supported yet"));
                        return;
                    }
        }
    }
    auto *binding = bindingFor(view, marking);
    if (!binding && marking)
    {
        emit statusChanged(tr("Custom table delegates are not supported yet"));
        return;
    }
    int count = 0;
    if (binding)
    {
        if (rowMode)
        {
            for (const auto &row : selectedRows)
            {
                if (marking) { binding->rows.insert(row, color); ++count; }
                else count += binding->rows.remove(row);
            }
        }
        else
        {
            for (const auto &cell : selectedCells)
            {
                if (marking) { binding->cells.insert(cell, color); ++count; }
                else count += binding->cells.remove(cell);
            }
        }
        view->viewport()->update();
    }
    if (rowMode)
        emit statusChanged(marking ? (count == 1 ? tr("1 row marked") : tr("%1 rows marked").arg(count))
                                   : (count == 1 ? tr("1 row cleared") : tr("%1 rows cleared").arg(count)));
    else
        emit statusChanged(marking ? (count == 1 ? tr("1 cell marked") : tr("%1 cells marked").arg(count))
                                   : (count == 1 ? tr("1 cell cleared") : tr("%1 cells cleared").arg(count)));
}

void AnnotationController::clearAll()
{
    auto *view = targetTable();
    if (!view)
    {
        emit statusChanged(tr("No result table selected"));
        return;
    }
    if (auto *binding = bindingFor(view, false))
    {
        binding->cells.clear();
        binding->rows.clear();
        view->viewport()->update();
    }
    emit statusChanged(tr("Annotations cleared"));
}

AnnotationController::TableBinding *AnnotationController::bindingFor(QTableView *view, bool create)
{
    std::erase_if(tables, [](const auto &binding) {
        return !binding->view || binding->model != binding->view->model() || binding->view->itemDelegate() != binding->delegate;
    });
    for (const auto &binding : tables)
        if (binding->view == view) return binding.get();
    // RTTI also distinguishes custom delegates that do not declare Q_OBJECT.
    if (!create || !view->itemDelegate() || typeid(*view->itemDelegate()) != typeid(QStyledItemDelegate))
        return nullptr;
    tables.push_back(std::make_unique<TableBinding>(view));
    return tables.back().get();
}

bool AnnotationController::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress)
        remember(qobject_cast<QWidget *>(watched));
    return false; // Observe the target only; all normal Qt interaction proceeds.
}
