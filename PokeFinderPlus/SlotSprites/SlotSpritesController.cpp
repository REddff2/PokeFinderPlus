#include "SlotSpritesController.hpp"
#include "PokemonControls.hpp"
#include "SlotSpriteStyle.hpp"
#include <PokeFinderPlus/PluginApi.hpp>
#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QPointer>
#include <QStyleFactory>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <algorithm>
#include <typeinfo>

struct SlotSpritesController::Binding
{
    QPointer<QTableView> view;
    QPointer<QStyle> original;
    std::unique_ptr<SlotSpriteStyle> style;
    bool explicitStyle;

    Binding(QTableView *table, QStyle *base, SpriteResolver &resolver, SpriteCache &cache)
        : view(table), original(table->style()), style(std::make_unique<SlotSpriteStyle>(base, resolver, cache)),
          explicitStyle(table->testAttribute(Qt::WA_SetStyle))
    {
        table->setStyle(style.get());
    }
    ~Binding()
    {
        if (!view) return;
        if (view->style() == style.get()) view->setStyle(explicitStyle ? original.data() : nullptr);
        view->viewport()->update();
    }
};
SlotSpritesController::SlotSpritesController()
    : resolver(PokeFinderPlus::pluginDataDirectory(QStringLiteral("SlotSprites")))
{
    if (!ready()) return;
    controls = std::make_unique<PokemonControls>(resolver, cache);
    scan();
    qApp->installEventFilter(this);
}
SlotSpritesController::~SlotSpritesController()
{
    if (qApp) qApp->removeEventFilter(this);
    controls.reset();
    bindings.clear();
    cache.clear();
}
void SlotSpritesController::queueScan()
{
    if (queued || scanning || !ready()) return;
    queued = true;
    QTimer::singleShot(0, this, [this] { queued = false; scan(); });
}
bool SlotSpritesController::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show || event->type() == QEvent::ChildPolished || event->type() == QEvent::StyleChange
        || event->type() == QEvent::Destroy)
        queueScan();
    else if (event->type() == QEvent::Paint)
    {
        if (auto *table = qobject_cast<QTableView *>(watched->parent()))
        {
            const auto it = std::find_if(bindings.begin(), bindings.end(), [table](const auto &b) { return b->view == table; });
            if ((it == bindings.end()) != !SlotSprites::supported(table->model())) queueScan();
        }
    }
    return false;
}
void SlotSpritesController::scan()
{
    if (scanning || !ready()) return;
    scanning = true;
    std::erase_if(bindings, [](const auto &b) {
        return !b->view || !SlotSprites::supported(b->view->model()) || b->view->style() != b->style.get();
    });
    for (QWidget *widget : QApplication::allWidgets())
    {
        auto *table = qobject_cast<QTableView *>(widget);
        if (!table || !SlotSprites::supported(table->model())) continue;
        if (std::any_of(bindings.begin(), bindings.end(), [table](const auto &b) { return b->view == table; })) continue;
        const auto *delegate = table->itemDelegate();
        if (!delegate || (typeid(*delegate) != typeid(QStyledItemDelegate)
            && QByteArray(typeid(*delegate).name()) != "class AnnotationDelegate")) continue;
        // Never transfer ownership of the application's original style to QProxyStyle.
        // Only clone known native factory styles; unknown/stylesheet styles are skipped.
        const QString key = table->style()->objectName();
        if (!QStyleFactory::keys().contains(key, Qt::CaseInsensitive)
            || QByteArray(table->style()->metaObject()->className()) == "QStyleSheetStyle") continue;
        if (auto *base = QStyleFactory::create(key))
        {
            if (typeid(*base) == typeid(*table->style())) bindings.push_back(std::make_unique<Binding>(table, base, resolver, cache));
            else delete base;
        }
    }
    scanning = false;
}
