#include "ResultIdentity.hpp"
#include "SpriteResolver.hpp"
#include "SpriteCache.hpp"
#include <PokeFinderPlus/PluginApi.hpp>
#include <PokeFinderPlus/Annotations/AnnotationController.hpp>
#include <PokeFinderPlus/IVTotal/IVTotalController.hpp>
#include <Core/Gen3/Generators/WildGenerator3.hpp>
#include <Core/Gen3/Encounters3.hpp>
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Item.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Util/Translator.hpp>
#include <Model/Gen3/WildModel3.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLibrary>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QVBoxLayout>
#include <iostream>
#include <stdexcept>
#include <algorithm>

void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
    std::cout << "PASS " << message << std::endl;
}
void events() { for (int i = 0; i < 5; ++i) QApplication::processEvents(); }
void coverage();
void renderCoverage();
QImage cell(QTableView &table, int row, int column, bool selected = false)
{
    QImage image(280, table.rowHeight(row), QImage::Format_ARGB32_Premultiplied);
    image.fill(table.palette().base().color());
    QPainter painter(&image);
    QStyleOptionViewItem option;
    option.initFrom(&table);
    option.widget = &table;
    option.rect = image.rect();
    option.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus | QStyle::State_MouseOver);
    option.state |= QStyle::State_Enabled | QStyle::State_Active;
    if (selected) option.state |= QStyle::State_Selected | QStyle::State_HasFocus;
    const auto index = table.model()->index(row, column);
    table.itemDelegateForIndex(index)->paint(&painter, option, index);
    return image;
}
int difference(const QImage &a, const QImage &b, int x0, int x1)
{
    int count = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = x0; x < qMin(x1, a.width()); ++x) count += a.pixel(x, y) != b.pixel(x, y);
    return count;
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    const int fontId = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisym.ttf");
    if (fontId >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(), 9));
    try
    {
        Translator::init("en");
        coverage();
        const auto directory = PokeFinderPlus::pluginDataDirectory("SlotSprites");
        SpriteResolver resolver(directory);
        check(resolver.ready(), "offline manifest loads from application directory");
        check(resolver.candidates({201, 26, 2, false, 0}).first().endsWith("unown-exclamation.png"), "explicit Unown punctuation form");
        check(resolver.candidates({550, 1, 0, false, 0}).first().endsWith("basculin-blue-striped.png"), "explicit Basculin form");
        check(resolver.candidates({592, 0, 1, true, 0}).first().contains("shiny/female/frillish.png"), "shiny female metadata mapping");
        check(resolver.candidates({0, -1, -1, false, 155}).first().endsWith("items/berry/oran.png"), "numeric Oran Berry mapping");
        check(resolver.candidates({9999, 0, 0, false, 0}).isEmpty(), "unknown species fails closed");
        check(resolver.candidates({25, 99, 0, true, 0}).first().endsWith("regular/pikachu.png"), "unknown form falls back to normal species");
        SpriteCache cache;
        check(cache.get({directory + "/missing.png"}, QSize(34,28), 1).isNull(), "missing PNG safely renders no icon");

        std::array<bool,25> natures; natures.fill(true);
        std::array<bool,16> powers; powers.fill(true);
        std::array<bool,13> encounterSlots; encounterSlots.fill(true);
        WildStateFilter filter(255,255,255,1,100,0,255,0,255,false, {0,0,0,0,0,0}, {31,31,31,31,31,31}, natures,powers,encounterSlots);
        Profile3 profile("Slot Sprites verification", Game::Emerald, 12345, 54321, false);
        const auto areas = Encounters3::getEncounters(Encounter::Grass, {false}, Game::Emerald);
        check(!areas.empty(), "real Emerald encounter areas loaded");
        WildGenerator3 generator(0,250000,0,Method::Method1,Lead::None,false,false,Item::None,areas.front(),profile,filter);
        const auto results = generator.generate(0);
        check(results.size() > 1000, "real WildGenerator3 RNG results generated");
        int shinyRow = -1;
        for (int row = 0; row < int(results.size()); ++row) if (results[row].isValid() && results[row].getShiny()) { shinyRow = row; break; }
        check(shinyRow >= 0, "real generated shiny result found");
        std::cout << "RESULTS " << results.size() << " SHINY_ROW " << shinyRow << std::endl;

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        QTableView table;
        table.setObjectName("tableViewGenerator");
        layout->addWidget(&table);
        auto *filterWidget = new QWidget(&window); filterWidget->setObjectName("filterGenerator");
        auto *stats = new QCheckBox(filterWidget); stats->setObjectName("checkBoxShowStats");
        WildGeneratorModel3 model(&table);
        model.addItems(results);
        QSortFilterProxyModel sorted;
        sorted.setSourceModel(&model);
        table.setModel(&sorted);
        table.setSortingEnabled(true);
        table.sortByColumn(-1, Qt::AscendingOrder);
        table.verticalHeader()->setDefaultSectionSize(30);
        table.resizeColumnsToContents();
        table.setColumnWidth(1, 150);
        window.resize(1300, 580);
        window.setWindowTitle("Slot Sprites — real Emerald Wild generator results");
        window.show(); events();
        auto *originalStyle = table.style();
        auto *originalDelegate = table.itemDelegate();
        const auto originalHeader = table.horizontalHeader()->saveState();
        const int originalHeight = table.rowHeight(0);
        const auto plain = cell(table,0,1);
        const auto plainSelected = cell(table,0,1,true);
        QTableView unrelated;
        QStandardItemModel unrelatedModel(2,2); unrelated.setModel(&unrelatedModel);
        auto *unrelatedStyle = unrelated.style(); unrelated.show(); events();
        QLibrary plugin(QDir(QCoreApplication::applicationDirPath()).filePath("SlotSprites.dll"));
        check(plugin.load(), "SlotSprites.dll loads");
        auto initialize = reinterpret_cast<PokeFinderPlusPluginInitialize>(plugin.resolve("pokefinder_plus_plugin_initialize"));
        auto shutdown = reinterpret_cast<PokeFinderPlusPluginShutdown>(plugin.resolve("pokefinder_plus_plugin_shutdown"));
        check(initialize && shutdown && initialize(&window), "headless plugin enabled"); events();
        QDir().mkpath(QDir(QCoreApplication::applicationDirPath()).filePath("evidence"));
        renderCoverage();
        check(table.style() != originalStyle && table.itemDelegate() == originalDelegate && table.model() == &sorted, "style hook only; original model and delegate retained");
        check(unrelated.style() == unrelatedStyle, "unrelated table untouched");
        check(table.rowHeight(0) == originalHeight && table.columnWidth(1) == 150, "row height and column width unchanged");
        const auto decorated = cell(table,0,1);
        const auto decoratedSelected = cell(table,0,1,true);
        check(difference(plain,decorated,0,36)>10, "sprite visibly rendered in real Slot cell");
        check(difference(plainSelected,decoratedSelected,170,278)==0, "native selected/focused background preserved");
        check(model.index(0,1).data() == sorted.index(0,1).data(), "original Slot text and roles unchanged");
        bool identitiesMatch = true;
        for(int row=0;row<1000;++row)
            identitiesMatch &= SlotSprites::identity(sorted.index(row,1)).species == results[row].getSpecie();
        check(identitiesMatch, "1000 sprite identities match generated species");
        auto shinyIdentity = SlotSprites::identity(sorted.index(shinyRow,1));
        check(shinyIdentity.shiny && resolver.candidates(shinyIdentity).first().contains("/shiny/"), "actual shiny result resolves shiny artwork");
        table.selectRow(shinyRow); table.scrollTo(sorted.index(shinyRow,1)); events();
        QDir().mkpath(QDir(QCoreApplication::applicationDirPath()).filePath("evidence"));
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("evidence/milestone-real-shiny.png"));

        table.sortByColumn(1,Qt::DescendingOrder); events();
        for(int row=0;row<50;++row)
        {
            auto source = sorted.mapToSource(sorted.index(row,1));
            if(SlotSprites::identity(sorted.index(row,1)).species != model.getItem(source.row()).getSpecie())
                throw std::runtime_error("Sorted sprite identity mismatch");
        }
        check(true,"sorting keeps sprites attached to original results");
        table.sortByColumn(-1,Qt::AscendingOrder); table.clearSelection(); table.scrollToTop(); events();
        auto iv = std::make_unique<IVTotalController>(); events();
        check(table.model()!=&sorted, "real IV Total controller active");
        check(SlotSprites::identity(table.model()->index(0,1)).species == results[0].getSpecie(), "identity crosses IV Total proxy chain");
        QWidget annotationWindow;
        auto annotations = std::make_unique<AnnotationController>(&annotationWindow);
        QString annotationStatus;
        QObject::connect(annotations.get(), &AnnotationController::statusChanged, [&](const QString &s){annotationStatus=s;});
        window.activateWindow(); table.setFocus(); events();
        table.setCurrentIndex(table.model()->index(0,1));
        table.selectionModel()->select(table.model()->index(0,1),QItemSelectionModel::ClearAndSelect);
        annotations->setColor(QColor(255,0,0,90)); annotations->mark(); events();
        check(annotationStatus == "1 cell marked", "Annotations accepts and marks Slot with all three plugins active");
        const auto tinted = cell(table,0,1);
        check(tinted.pixelColor(270,10)!=decorated.pixelColor(270,10), "annotation tint reaches native background");
        check(difference(tinted,decorated,0,36)>0,"annotation plus sprite render together");
        window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("evidence/milestone-all-three.png"));
        shutdown(); events();
        check(table.style()==originalStyle, "disable restores original style with Annotations and IV Total active");
        const auto tintedPlain = cell(table,0,1);
        check(difference(tinted,tintedPlain,0,36)>10 && difference(tinted,tintedPlain,170,278)==0, "disable removes only sprite and preserves annotation tint");
        check(initialize(&window),"re-enable while both other plugins active"); events();
        check(difference(cell(table,0,1),tinted,0,278)==0,"re-enable restores identical annotated sprite rendering");
        annotations.reset(); events(); iv.reset(); events();
        shutdown(); events();
        check(table.style()==originalStyle && table.itemDelegate()==originalDelegate && table.model()==&sorted, "all three teardown restores original objects");
        check(table.columnWidth(1)==150 && table.rowHeight(0)==originalHeight,"original dimensions restored");
        check(difference(cell(table,0,1),plain,0,278)==0,"disable returns pixel-identical original rendering");
        check(initialize(&window),"enable for late-window and reset checks");
        {
            QTableView late;
            WildGeneratorModel3 lateModel(&late); lateModel.addItems({results.front()}); late.setModel(&lateModel);
            auto *lateOriginal=late.style(); late.show(); events();
            check(late.style()!=lateOriginal,"table opened after activation gains sprites");
        }
        events();
        model.clearModel(); model.addItems({results[shinyRow]}); events();
        check(SlotSprites::identity(table.model()->index(0,1)).shiny,"new search/model reset updates sprite identity");
        shutdown(); events();
        const auto lifecycleBaseline=cell(table,0,1);
        std::array<int,3> startOrder{0,1,2};
        int sequences=0;
        do
        {
            std::array<int,3> stopOrder{0,1,2};
            do
            {
                for(int pluginId:startOrder)
                {
                    if(pluginId==0) { if(!initialize(&window)) throw std::runtime_error("Lifecycle initialize failed"); }
                    if(pluginId==1) iv=std::make_unique<IVTotalController>();
                    if(pluginId==2) annotations=std::make_unique<AnnotationController>(&annotationWindow);
                    events();
                }
                QString status;
                QObject::connect(annotations.get(),&AnnotationController::statusChanged,[&](const QString &s){status=s;});
                window.activateWindow(); table.setFocus(); table.setCurrentIndex(table.model()->index(0,1)); events();
                table.selectionModel()->select(table.model()->index(0,1),QItemSelectionModel::ClearAndSelect);
                annotations->mark(); events();
                if(status!="1 cell marked") throw std::runtime_error("Lifecycle Annotations rejected Slot");
                for(int pluginId:stopOrder)
                {
                    if(pluginId==0) shutdown();
                    if(pluginId==1) iv.reset();
                    if(pluginId==2) annotations.reset();
                    events();
                }
                if(table.style()!=originalStyle || table.itemDelegate()!=originalDelegate || table.model()!=&sorted
                   || table.columnWidth(1)!=150 || cell(table,0,1)!=lifecycleBaseline)
                    throw std::runtime_error("Activation/shutdown permutation failed restoration");
                ++sequences;
            } while(std::next_permutation(stopOrder.begin(),stopOrder.end()));
        } while(std::next_permutation(startOrder.begin(),startOrder.end()));
        check(sequences==36,"all 36 activation/shutdown order combinations restore original rendering and dimensions");
        check(plugin.unload(),"DLL unload after shutdown succeeds");
        std::cout << "ALL MILESTONE CHECKS PASSED" << std::endl;
        return 0;
    }
    catch(const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<std::endl; return 1; }
}
