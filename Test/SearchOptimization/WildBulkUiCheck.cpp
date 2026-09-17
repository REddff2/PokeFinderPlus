#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/CheckList.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <Form/Controls/ComboBoxProxy.hpp>
#include <Form/Controls/Filter.hpp>
#include <Form/Gen5/Wild5.hpp>
#include <Form/Util/Settings.hpp>
#include <Model/Gen5/WildModel5.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <iostream>
#include <stdexcept>
#include <tuple>

static void check(bool ok,const char *text)
{ if(!ok) throw std::runtime_error(text); std::cout<<"PASS "<<text<<std::endl; }
template<class T> static T *child(QObject &owner,const char *name)
{ auto *found=owner.findChild<T*>(name); if(!found) throw std::runtime_error(name); return found; }
static auto fields(const SearcherState5<WildState5> &r)
{
    const auto &s=r.getState();
    return std::tuple(r.getInitialSeed(),r.getDateTime(),r.getTimer0(),r.getButtons(),s.getEC(),s.getPID(),s.getStats(),
        s.getAbilityIndex(),s.getIVs(),s.getAbility(),s.getCharacteristic(),s.getGender(),s.getHiddenPower(),
        s.getHiddenPowerStrength(),s.getLevel(),s.getNature(),s.getShiny(),s.getAdvances(),s.getIVAdvances(),
        s.getEncounterSlot(),s.getSpecie(),s.getForm(),s.getItem(),s.getLead(),s.getLeadMask(),s.getMovingTrigger(),
        s.getMovingSteps(),s.getPhenomenon(),s.getPhenomenonItem(),s.isValid(),static_cast<const WildGeneratorState&>(s).isValid(),
        s.getPassPower(),s.getVariableNature(),s.getLeadRequired(),s.getChatot(),s.getNeedle());
}

int main(int argc,char **argv)
{
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf"); app.setFont(QFont("Segoe UI",9)); app.setStyle("fusion");
    app.setOrganizationName("WildBulkVerification"); app.setApplicationName("WildBulkVerification");
    QTemporaryDir temporary;
    if(!temporary.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QFile profiles(temporary.filePath("profiles.json")); if(!profiles.open(QIODevice::WriteOnly)) return 2;
    profiles.write("{}");profiles.close(); ProfileLoader::init(temporary.filePath("profiles.json").toStdWString());
    ProfileLoader5::setProfiles({Profile5("Wild bulk level check",Game::Black,12345,54321,"","",0x001122334455ULL,
        {true,false,false,false,false,false,false,false,false},0x60,6,6,false,0x1100,0x1100,false,false,DSType::DS,Language::English)});
    QSettings().setValue("settings/threads",24); QSettings().setValue("settings/locale","en");
    Q_INIT_RESOURCE(resources); Translator::init("en");

    try
    {
        Settings settings; settings.setAttribute(Qt::WA_DeleteOnClose,false);
        auto *pruning=child<QCheckBox>(settings,"checkBoxSearchPruning");
        check(!pruning->isChecked(),"native smart search defaults off");
        Wild5 wild; wild.setAttribute(Qt::WA_DeleteOnClose,false); wild.show(); wild.resize(1400,900); QTest::qWait(150);
        check(wild.hasProfiles(),"isolated normal Wild profile loaded");
        child<QTabWidget>(wild,"tabRNGSelector")->setCurrentIndex(1);
        child<QComboBox>(wild,"comboBoxSearcherEncounter")->setCurrentIndex(0);
        auto *locations=child<ComboBoxProxy>(wild,"comboBoxSearcherLocation");
        locations->setCurrentIndexByData(41);
        check(locations->getCurrentUShort()==41,"normal Chargestone Cave 1F area present");
        child<ComboMenu>(wild,"comboMenuSearcherLead")->setCheckedData({255});
        child<QCheckBox>(wild,"checkBoxSearcherMovingTrigger")->setChecked(false);
        child<QCheckBox>(wild,"checkBoxSearcherSwarm")->setChecked(false);
        for(const char *name:{"textBoxSearcherInitialIVAdvances","textBoxSearcherMaxIVAdvances","textBoxSearcherInitialAdvances"})
            child<QLineEdit>(wild,name)->setText("0");
        child<QLineEdit>(wild,"textBoxSearcherMaxAdvances")->setText("999");
        for(const char *name:{"dateEditSearcherStartDate","dateEditSearcherEndDate"}) child<QDateEdit>(wild,name)->setDate(QDate(2012,10,7));
        auto *filter=child<Filter>(wild,"filterSearcher");
        int ivCount=0;
        for(auto *spin:filter->findChildren<QSpinBox*>())
            if(spin->objectName().endsWith("Min") && spin->maximum()==31) {spin->setValue(10);++ivCount;}
        check(ivCount==6,"all six IV ranges set to 10-31");
        std::array<bool,16> hp; hp.fill(true); child<CheckList>(*filter,"checkListHiddenPower")->setChecks(hp);
        check(filter->getHiddenPowers()==hp,"all Hidden Power types selected");
        child<QSpinBox>(*filter,"spinBoxLevelMin")->setValue(27);
        child<QSpinBox>(*filter,"spinBoxLevelMax")->setValue(27);
        check(filter->getLevelMin()==27 && filter->getLevelMax()==27,"active level filter is 27-27");
        check(child<QLabel>(wild,"labelIVFastSearch")->text().contains("does not have a IV cache"),"ordinary search path selected without caches");
        auto *search=child<QPushButton>(wild,"pushButtonSearch"); auto *cancel=child<QPushButton>(wild,"pushButtonCancel");
        auto *model=wild.findChild<WildSearcherModel5*>(); check(model,"normal Wild result model available");
        auto run=[&](bool enabled) {
            pruning->setChecked(enabled); check(SearchOptimization::pruningEnabled()==enabled,"native setting updates Wild policy");
            QElapsedTimer timer;timer.start();search->click();
            check(!search->isEnabled() && cancel->isEnabled(),"ordinary Wild UI search started");
            while(!search->isEnabled() && timer.elapsed()<60000) QTest::qWait(50);
            check(search->isEnabled() && child<QProgressBar>(wild,"progressBar")->value()==100,"ordinary Wild UI search completed");
            std::cout<<"UI_TIMING optimized="<<enabled<<" ms="<<timer.elapsed()<<" results="<<model->rowCount()<<std::endl;
            return model->getModel();
        };
        auto baseline=run(false); auto optimized=run(true);
        check(!baseline.empty() && optimized.size()==baseline.size(),"bulk level setup returns nonempty identical-count results");
        auto order=[](const auto &a,const auto &b){return fields(a)<fields(b);};
        std::sort(baseline.begin(),baseline.end(),order);std::sort(optimized.begin(),optimized.end(),order);
        bool equal=true;for(size_t i=0;i<baseline.size();++i) equal&=fields(baseline[i])==fields(optimized[i]);
        check(equal,"all fields from the real Wild UI match with optimization off/on");
        QElapsedTimer sortTimer; sortTimer.start();
        child<QTableView>(wild,"tableViewSearcher")->model()->sort(0,Qt::AscendingOrder);
        QCoreApplication::processEvents();
        std::cout<<"UI_EXPLICIT_SORT ms="<<sortTimer.elapsed()<<" rows="<<model->rowCount()<<std::endl;
        if(argc>1) wild.grab().save(QString::fromLocal8Bit(argv[1])+"/bulk-wild-test.png");
        search->click(); cancel->click();
        for(int i=0;i<100 && !search->isEnabled();++i) QTest::qWait(50);
        check(search->isEnabled() && !cancel->isEnabled(),"ordinary Wild UI cancels cleanly");
        wild.close();std::cout<<"WILD_UI_PASS exit=0"<<std::endl;return 0;
    }
    catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<std::endl;return 1;}
}
