#include <Core/Enum/Game.hpp>
#include <Core/Gen3/Profile3.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Gen3/Wild3.hpp>
#include <Form/Gen4/PokeRadar.hpp>
#include <Form/Util/Settings.hpp>
#include <PokeFinderPlus/PluginManager.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QTableView>
#include <QTabWidget>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char *message)
{
    if(!ok) throw std::runtime_error(message);
    std::cout << "PASS " << message << std::endl;
}
static void pump() { QTest::qWait(150); }

int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    // The offscreen Windows platform does not enumerate system fonts itself.
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    app.setFont(QFont("Segoe UI",9));
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("SearchOptimizationVerification");
    app.setApplicationName("SearchOptimizationVerification");
    QTemporaryDir temp(QDir(QCoreApplication::applicationDirPath()).filePath("data/SearchOptimizationVerification-XXXXXX"));
    if(!temp.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    QFile profiles(temp.filePath("profiles.json"));
    if(!profiles.open(QIODevice::WriteOnly)) return 2;
    profiles.write("{}"); profiles.close();
    ProfileLoader::init(temp.filePath("profiles.json").toStdWString());
    ProfileLoader3::setProfiles({Profile3("Emerald",Game::Emerald,12345,54321,false)});
    ProfileLoader4::setProfiles({Profile4("Diamond",Game::Diamond,12345,54321,true)});
    QSettings().setValue("settings/threads",24);
    QSettings().setValue("settings/locale","en");
    Q_INIT_RESOURCE(resources);
    Translator::init("en");
    app.setStyle("fusion");
    try
    {
        Settings settings;
        settings.setAttribute(Qt::WA_DeleteOnClose,false);
        settings.show(); pump();
        auto *prune=settings.findChild<QCheckBox*>("checkBoxSearchPruning");
        auto *gpu=settings.findChild<QCheckBox*>("checkBoxSearchGpu");
        check(prune && gpu && !settings.findChild<QCheckBox*>("checkBoxSearchWorkerLimit"),"native Smart/GPU controls exist without the retired worker checkbox");
        check(!prune->isChecked() && !gpu->isChecked() && !gpu->isEnabled(),"clean install defaults OFF/OFF");
        prune->setChecked(true);check(gpu->isEnabled(),"Smart enables GPU checkbox");
        gpu->setChecked(true);check(SearchOptimization::gpuEnabled(),"GPU opt-in persisted and enabled");
        prune->setChecked(false);
        check(!gpu->isChecked() && !gpu->isEnabled() && !SearchOptimization::gpuEnabled(),"Smart off immediately forces GPU off");
        check(!QSettings().value("settings/plusSearchGpu").toBool(),"GPU false persisted");
        prune->setChecked(true);
        check(!gpu->isChecked() && SearchOptimization::pruningEnabled(),"Smart CPU re-enabled without reviving GPU");
        check(QSettings().value("settings/plusSearchPruning").toBool(),"existing smart persistence key preserved");
        settings.grab().save(temp.filePath("settings.png"));
        if(argc>1) settings.grab().save(QString::fromLocal8Bit(argv[1])+"/settings.png");
        settings.hide();

        QWidget host; host.show();
        PluginManager manager(&host); pump();
        QAction *annotations=nullptr,*ivTotal=nullptr,*sprites=nullptr;
        for(auto *action:manager.menu()->actions())
        {
            if(action->text()=="Annotations") annotations=action;
            if(action->text()=="IV Total") ivTotal=action;
            if(action->text()=="Slot Sprites") sprites=action;
        }
        check(annotations && ivTotal && sprites,"all three deployed plugin DLLs discovered");
        for(auto *action:{annotations,ivTotal,sprites}) if(!action->isChecked()) { action->trigger(); pump(); }
        check(annotations->isChecked() && ivTotal->isChecked() && sprites->isChecked(),"all three deployed plugins initialize");

        Wild3 wild; wild.setAttribute(Qt::WA_DeleteOnClose,false); wild.show(); pump();
        auto *seed=wild.findChild<QLineEdit*>("textBoxGeneratorSeed");
        auto *maximum=wild.findChild<QLineEdit*>("textBoxGeneratorMaxAdvances");
        check(seed && maximum,"existing Wild3 controls");
        seed->setText("0"); maximum->setText("9");
        check(QMetaObject::invokeMethod(&wild,"generate",Qt::DirectConnection),"existing Wild3 generation invoked"); pump();
        auto *view=wild.findChild<QTableView*>("tableViewGenerator");
        check(view && view->model()->rowCount()>0,"existing search results generated");
        int total=-1;
        for(int c=0;c<view->model()->columnCount();++c)
            if(view->model()->headerData(c,Qt::Horizontal).toString()=="IV Total") total=c;
        check(total>=6,"IV Total attaches to generated results");
        for(int r=0;r<view->model()->rowCount();++r)
        {
            int sum=0;
            for(int c=total-6;c<total;++c) sum+=view->model()->index(r,c).data().toInt();
            check(view->model()->index(r,total).data().toInt()==sum,"IV Total arithmetic");
        }
        view->model()->sort(total,Qt::DescendingOrder); pump();
        bool decorated=false;
        for(auto *combo:wild.findChildren<QComboBox*>())
            for(int r=0;r<combo->count();++r)
                decorated |= !combo->itemData(r,Qt::DecorationRole).value<QIcon>().isNull();
        check(decorated,"Slot Sprites decorates actual Pokemon controls");

        view->setCurrentIndex(view->model()->index(0,0)); view->selectRow(0); view->setFocus();
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(view->currentIndex()).center()); pump();
        QPushButton *mark=nullptr;
        for(auto *window:QApplication::topLevelWidgets())
            if(window->windowTitle()=="Annotations")
                for(auto *button:window->findChildren<QPushButton*>()) if(button->text()=="Mark") mark=button;
        check(mark,"Annotations Mark control available");
        mark->click(); pump();
        bool marked=false;
        for(auto *window:QApplication::topLevelWidgets())
            for(auto *label:window->findChildren<QLabel*>()) marked |= label->text().contains("marked");
        check(marked,"Annotations marks actual generated selection");
        if(argc>1) wild.grab().save(QString::fromLocal8Bit(argv[1])+"/plugins.png");
        for(auto *action:{annotations,ivTotal,sprites}) { action->trigger(); pump(); }
        check(!annotations->isChecked() && !ivTotal->isChecked() && !sprites->isChecked(),"all plugins disable cleanly");
        wild.close(); host.close();
        {
            // Exercise normal Radar activation dispatch and cancellation.

            PokeRadar radar; radar.setAttribute(Qt::WA_DeleteOnClose,false); radar.show(); pump();
            auto *tabs=radar.findChild<QTabWidget*>();
            check(tabs && tabs->count()>=2,"Radar search tab exists");
            tabs->setCurrentIndex(1);
            auto *tab=tabs->widget(1);
            QGroupBox *info=nullptr;
            for(auto *box:tab->findChildren<QGroupBox*>()) if(box->title()=="RNG Info") info=box;
            check(info,"Radar search RNG controls");
            auto *layout=qobject_cast<QGridLayout*>(info->layout());
            auto textAt=[&](int row,const QString &text) {
                auto *edit=qobject_cast<QLineEdit*>(layout->itemAtPosition(row,1)->widget());
                check(edit,"Radar seed-range control"); edit->setText(text);
            };
            textAt(1,"600"); textAt(2,"600"); textAt(3,"0"); textAt(4,"0");
            qobject_cast<QSpinBox*>(layout->itemAtPosition(5,1)->widget())->setValue(1);
            qobject_cast<QSpinBox*>(layout->itemAtPosition(6,1)->widget())->setValue(0);
            for(auto *spin:tab->findChildren<QSpinBox*>()) if(spin->objectName().endsWith("Min") && spin->maximum()==31) spin->setValue(31);
            auto *search=qobject_cast<QPushButton*>(layout->itemAtPosition(7,0)->widget());
            auto *cancel=qobject_cast<QPushButton*>(layout->itemAtPosition(7,1)->widget());
            auto *progress=tab->findChild<QProgressBar*>();
            check(search && cancel && progress,"Radar search lifecycle controls");
            search->click();
            check(!search->isEnabled(),"Radar normal activation request starts");
            for(int i=0;i<70 && !search->isEnabled();++i) QTest::qWait(100);
            check(search->isEnabled() && progress->value()==100,"Radar completes every activation");
            for(auto *spin:tab->findChildren<QSpinBox*>()) if(spin->objectName().endsWith("Min") && spin->maximum()==31) spin->setValue(0);
            search->click(); check(cancel->isEnabled(),"Radar cancellation available"); cancel->click();
            for(int i=0;i<50 && !search->isEnabled();++i) QTest::qWait(100);
            check(search->isEnabled() && !cancel->isEnabled(),"Radar cancels all active activations");
            radar.close();
        }
        std::cout << "UI_AND_PLUGINS_PASS exit=0" << std::endl;
        return 0;
    }
    catch(const std::exception &e) { std::cerr << "FAIL " << e.what() << std::endl; return 1; }
}
