// Temporary TEST-only probe. Never included in the distributable plugin package.
#include <PokeFinderPlus/PluginApi.hpp>
#include "ResultIdentity.hpp"
#include "SpriteResolver.hpp"
#include <QApplication>
#include <QAction>
#include <QAbstractItemDelegate>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <stdexcept>
void runControlChecks(QWidget *host);
void runBroadChecks(QWidget *host);

namespace
{
QFile logFile;
QString output;
void log(const QString &message)
{
    QTextStream stream(&logFile); stream << message << '\n'; stream.flush(); logFile.flush();
}
void require(bool condition, const QString &message)
{
    if (!condition) throw std::runtime_error(message.toStdString());
    log("PASS " + message);
}
void settle() { for(int n=0;n<6;++n) QApplication::processEvents(); }
QAction *action(QWidget *host,const QString &text)
{
    for(auto *a:host->findChildren<QAction *>()) if(a->text()==text) return a;
    throw std::runtime_error(("Action missing: "+text).toStdString());
}
void enabled(QAction *a,bool state)
{
    if(a->isChecked()!=state) a->trigger();
    settle(); require(a->isChecked()==state,a->text()+" checkmark "+(state?"enabled":"disabled"));
}
template<class T> T *child(QObject *parent,const char *name)
{
    auto *result=parent->findChild<T *>(name);
    if(!result) throw std::runtime_error(std::string("Missing widget ")+name);
    return result;
}
QImage paint(QTableView *view,int row,int column)
{
    QImage image(280,view->rowHeight(row),QImage::Format_ARGB32_Premultiplied); image.fill(view->palette().base().color());
    QPainter painter(&image); QStyleOptionViewItem option; option.initFrom(view);
    option.widget=view; option.rect=image.rect();
    option.state &= ~(QStyle::State_Selected|QStyle::State_HasFocus|QStyle::State_MouseOver);
    option.state |= QStyle::State_Enabled|QStyle::State_Active;
    auto index=view->model()->index(row,column); view->itemDelegateForIndex(index)->paint(&painter,option,index); return image;
}
void run(QWidget *host)
{
    try
    {
        auto *sprites=action(host,"Slot Sprites"); auto *iv=action(host,"IV Total"); auto *annotations=action(host,"Annotations");
        enabled(sprites,false); enabled(iv,false); enabled(annotations,false);
        child<QPushButton>(host,"pushButtonWild3")->click(); settle();
        QWidget *wild=nullptr;
        for(auto *w:QApplication::topLevelWidgets()) if(QByteArray(w->metaObject()->className())=="Wild3") wild=w;
        require(wild,"real Gen 3 Wild window opened in frozen TEST executable");
        auto *view=child<QTableView>(wild,"tableViewGenerator");
        const auto *originalModel=view->model(); auto *originalDelegate=view->itemDelegate(); auto *originalStyle=view->style();
        const int originalHeight=view->verticalHeader()->defaultSectionSize();
        child<QLineEdit>(wild,"textBoxGeneratorSeed")->setText("0");
        child<QLineEdit>(wild,"textBoxGeneratorInitialAdvances")->setText("0");
        child<QLineEdit>(wild,"textBoxGeneratorMaxAdvances")->setText("100000");
        child<QLineEdit>(wild,"textBoxGeneratorOffset")->setText("0");
        require(QMetaObject::invokeMethod(wild,"generate",Qt::DirectConnection),"real window Generate slot invoked"); settle();
        require(view->model()->rowCount()>100,"real results generated in TEST application");
        log("RESULTS "+QString::number(view->model()->rowCount())+" STYLE "+originalStyle->metaObject()->className()+" "+originalStyle->objectName());
        const auto plain=paint(view,0,1); const auto text=view->model()->index(0,1).data(); const int width=view->columnWidth(1);
        enabled(sprites,true);
        require(view->style()!=originalStyle && view->model()==originalModel && view->itemDelegate()==originalDelegate,"existing real table gains style hook only");
        const auto decorated=paint(view,0,1);
        require(decorated!=plain && view->model()->index(0,1).data()==text,"actual sprite paints beside unchanged Slot text");
        require(view->columnWidth(1)==width && view->verticalHeader()->defaultSectionSize()==originalHeight,"host dimensions unchanged");
        SpriteResolver resolver(PokeFinderPlus::pluginDataDirectory("SlotSprites"));
        int shinyRow=-1;
        for(int row=0;row<view->model()->rowCount();++row)
        {
            auto identity=SlotSprites::identity(view->model()->index(row,1));
            if(identity.shiny) { shinyRow=row; require(resolver.candidates(identity).first().contains("/shiny/"),"real TEST shiny result maps shiny PNG"); break; }
        }
        require(shinyRow>=0,"TEST search contains shiny result");
        view->selectRow(shinyRow); view->scrollTo(view->model()->index(shinyRow,1)); settle();
        wild->resize(1400,800); settle(); wild->grab().save(output+"/host-real-shiny.png");
        enabled(iv,true); enabled(annotations,true);
        QWidget *annotationWindow=nullptr;
        for(auto *w:QApplication::topLevelWidgets()) if(w->windowTitle()=="Annotations") annotationWindow=w;
        require(annotationWindow,"actual Annotations plugin window opened");
        wild->activateWindow(); view->setFocus(); settle();
        view->selectionModel()->select(view->model()->index(0,1),QItemSelectionModel::ClearAndSelect);
        bool marked=false;
        for(auto *button:annotationWindow->findChildren<QPushButton *>()) if(button->text()=="Mark") { button->click(); marked=true; break; }
        require(marked,"actual Mark button clicked"); settle();
        require(child<QLabel>(annotationWindow,"annotationStatus")->text()=="1 cell marked","Annotations accepts Slot with all three deployed DLLs active");
        auto tinted=paint(view,0,1);
        require(tinted.pixelColor(270,10)!=decorated.pixelColor(270,10),"actual annotation tint preserved");
        view->scrollToTop(); settle(); wild->grab().save(output+"/host-all-three.png");
        enabled(sprites,false);
        auto tintedPlain=paint(view,0,1);
        require(tintedPlain!=tinted && tintedPlain.pixelColor(270,10)==tinted.pixelColor(270,10),"disable removes sprite and retains actual annotation tint");
        enabled(sprites,true); require(paint(view,0,1)==tinted,"re-enable restores annotated sprite");
        view->sortByColumn(1,Qt::DescendingOrder); settle();
        require(SlotSprites::identity(view->model()->index(0,1)).species>0,"sorted result still resolves through actual IV Total proxies");
        QElapsedTimer timer; timer.start();
        for(int n=0;n<100;++n) { view->verticalScrollBar()->setValue(n*97); view->viewport()->repaint(); settle(); }
        log("SCROLL_100_STEPS_MS "+QString::number(timer.elapsed()));
        enabled(annotations,false); enabled(sprites,false); enabled(iv,false);
        require(view->style()==originalStyle && view->model()==originalModel && view->itemDelegate()==originalDelegate,"actual plugins restore original objects in alternate shutdown order");
        require(view->columnWidth(1)==width,"no stale IV Total header width after Slot Sprites disabled first");
        enabled(sprites,true);
        child<QPushButton>(host,"pushButtonWild4")->click(); settle();
        bool late=false;
        for(auto *w:QApplication::allWidgets()) if(auto *table=qobject_cast<QTableView *>(w))
            if(SlotSprites::supported(table->model()) && table!=view && QByteArray(SlotSprites::rootModel(table->model())->metaObject()->className())=="WildGeneratorModel4")
                late=table->style()!=originalStyle;
        require(late,"Gen 4 window opened after activation receives hook");
        child<QLineEdit>(wild,"textBoxGeneratorMaxAdvances")->setText("100");
        QMetaObject::invokeMethod(wild,"generate",Qt::DirectConnection); settle();
        require(view->model()->rowCount()>0 && view->model()->rowCount()<=101 && SlotSprites::identity(view->model()->index(0,1)).species>0,"actual new search updates sprites");
        runControlChecks(host);
        runBroadChecks(host);
        log("HOST PROBE PASSED — requesting normal application shutdown");
        logFile.close();
        QCoreApplication::quit();
    }
    catch(const std::exception &e)
    {
        log(QString("FAIL ")+e.what()); logFile.close(); QCoreApplication::exit(1);
    }
}
}
extern "C" __declspec(dllexport) unsigned int pokefinder_plus_plugin_api_version() { return POKEFINDER_PLUS_PLUGIN_API_VERSION; }
extern "C" __declspec(dllexport) const char *pokefinder_plus_plugin_name() { return "Slot Sprites TEST probe"; }
extern "C" __declspec(dllexport) bool pokefinder_plus_plugin_initialize(QWidget *host)
{
    if(qEnvironmentVariable("SLOT_SPRITES_VERIFY_HOST")!="1") return false;
    output=PokeFinderPlus::pluginDataDirectory("SlotSprites")+"/verification"; QDir().mkpath(output);
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,output+"/settings");
    const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisym.ttf");
    if(font>=0) qApp->setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(),9));
    logFile.setFileName(output+"/host-probe.log"); if(!logFile.open(QIODevice::WriteOnly|QIODevice::Truncate)) return false;
    QTimer::singleShot(300,qApp,[host]{run(host);});
    QTimer::singleShot(180000,qApp,[]{log("FAIL probe timeout");QCoreApplication::exit(2);});
    return true;
}
extern "C" __declspec(dllexport) void pokefinder_plus_plugin_shutdown() {}
