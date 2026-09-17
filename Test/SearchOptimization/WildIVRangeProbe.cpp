#include "IVRangeProbe.hpp"
#include "IVDomainCases.hpp"
#include <iostream>
#include <fstream>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Buttons.hpp>
#include <Core/Gen5/EncounterArea5.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/Keypresses.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/CheckList.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <Form/Controls/ComboBoxProxy.hpp>
#include <Form/Controls/Filter.hpp>
#include <Form/Gen5/Wild5.hpp>
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
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
using json=nlohmann::json;
static void check(bool ok,const char *text){if(!ok)throw std::runtime_error(text);std::cout<<"PASS "<<text<<std::endl;}
template<class T> static T *child(QObject &o,const char *name){auto p=o.findChild<T*>(name);if(!p)throw std::runtime_error(name);return p;}
static json fields(const SearcherState5<WildState5> &r){
    const auto &s=r.getState();
    return json{{"seed",r.getInitialSeed()},{"date",r.getDateTime().toString()},{"timer0",r.getTimer0()},{"buttons",toInt(r.getButtons())},
        {"ivs",s.getIVs()},{"stats",s.getStats()},
        {"fields",{s.getEC(),s.getPID(),s.getAbilityIndex(),s.getAbility(),s.getCharacteristic(),s.getGender(),s.getHiddenPower(),
            s.getHiddenPowerStrength(),s.getLevel(),s.getNature(),s.getShiny(),s.getAdvances(),s.getIVAdvances(),s.getEncounterSlot(),s.getSpecie(),
            s.getForm(),s.getItem(),toInt(s.getLead()),s.getLeadMask(),s.getMovingTrigger(),s.getMovingSteps(),s.getPhenomenon(),s.getPhenomenonItem(),
            s.isValid(),static_cast<const WildGeneratorState&>(s).isValid(),s.getPassPower(),s.getVariableNature(),s.getLeadRequired(),s.getChatot(),s.getNeedle()}}};
}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    const std::string mode=argc>1?argv[1]:"--subset";
    const QString output=argc>2?QString::fromLocal8Bit(argv[2]):QDir::currentPath();
    const int requested=QThread::idealThreadCount();
    if(argc!=5) return 2; // mode, output directory, IV cache, SHA cache
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");app.setFont(QFont("Segoe UI",9));app.setStyle("fusion");
    app.setOrganizationName("IVRangeProbe");app.setApplicationName("IVRangeProbe");
    QTemporaryDir temporary(QDir(output).filePath("IVRangeProbe-XXXXXX"));if(!temporary.isValid())return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    QFile profileFile(temporary.filePath("profiles.json"));if(!profileFile.open(QIODevice::WriteOnly))return 3;profileFile.write("{}");profileFile.close();
    ProfileLoader::init(temporary.filePath("profiles.json").toStdWString());
    QSettings().setValue("settings/threads",requested);QSettings().setValue("settings/locale","en");
    Q_INIT_RESOURCE(resources);Translator::init("en");

#ifdef REAL_WILD_PROFILE
    RealWildMetrics::calibrateTimers();
#endif
    try{
        std::array<bool,9> pressed;pressed.fill(true);
        Profile5 p("Smart Search fixture",Game::White,69,58008,argv[3],argv[4],0x1656a634e5ULL,
            pressed,0x60,6,8,false,0xc7f,0xc7f,false,false,DSType::DS,Language::English,false);
        ProfileLoader5::setProfiles({p});
        auto keys=Keypresses::getKeypresses(p);
        const int days=1; // Dispatch only: no search workers are started.
        std::cout<<"DOMAIN dispatch-only days=1 workers_started=0"<<std::endl;
        auto areas=Encounters5::getEncounters(Encounter::Grass,{false,0},&p);
        auto a=std::ranges::find_if(areas,[](const auto &a){return a.getLocation()==8;});
        check(a!=areas.end() && a->getPokemon(5).getSpecie()==561,"White Desert Resort Desert slot 5 is Sigilyph");
        Wild5 wild;wild.setAttribute(Qt::WA_DeleteOnClose,false);wild.show();wild.resize(1400,900);QTest::qWait(100);
        check(wild.hasProfiles(),"real profile loaded into isolated Wild UI");
        child<QTabWidget>(wild,"tabRNGSelector")->setCurrentIndex(1);
        child<QComboBox>(wild,"comboBoxSearcherEncounter")->setCurrentIndex(0);
        child<QComboBox>(wild,"comboBoxSearcherSeason")->setCurrentIndex(0);
        child<ComboBoxProxy>(wild,"comboBoxSearcherLocation")->setCurrentIndexByData(8);
        auto pokemon=child<QComboBox>(wild,"comboBoxSearcherPokemon");pokemon->setCurrentIndex(pokemon->findText("Sigilyph"));
        check(pokemon->currentText()=="Sigilyph","Sigilyph selected");
        child<ComboMenu>(wild,"comboMenuSearcherLead")->setCheckedData({255});
        child<QCheckBox>(wild,"checkBoxSearcherMovingTrigger")->setChecked(false);
        child<QCheckBox>(wild,"checkBoxSearcherSwarm")->setChecked(false);
        for(const char *name:{"textBoxSearcherInitialIVAdvances","textBoxSearcherMaxIVAdvances","textBoxSearcherInitialAdvances","textBoxSearcherMaxAdvances"})child<QLineEdit>(wild,name)->setText("0");
        child<QDateEdit>(wild,"dateEditSearcherStartDate")->setDate(QDate(2026,6,15));
        child<QDateEdit>(wild,"dateEditSearcherEndDate")->setDate(QDate(2026,6,15).addDays(days-1));
        auto filter=child<Filter>(wild,"filterSearcher");
        int ivCount=0;for(auto spin:filter->findChildren<QSpinBox*>())if(spin->objectName().endsWith("Min") && spin->maximum()==31){spin->setValue(25);++ivCount;}
        std::array<bool,13> encounterSlots{};encounterSlots[5]=true;child<CheckList>(*filter,"checkListEncounterSlot")->setChecks(encounterSlots);
        // The normal UI pads the unused swarm slot to true; swarm is disabled.
        encounterSlots[12]=true;check(a->getPokemon(12).getSpecie()==0,"no swarm slot in the selected area");
        child<QSpinBox>(*filter,"spinBoxLevelMin")->setValue(20);child<QSpinBox>(*filter,"spinBoxLevelMax")->setValue(20);
        child<QComboBox>(*filter,"comboBoxShiny")->setCurrentIndex(3);
        std::cout<<"FILTER_READBACK iv_count="<<ivCount<<" shiny="<<int(filter->getShiny())<<" level="<<int(filter->getLevelMin())<<':'<<int(filter->getLevelMax())<<" slots=";for(bool b:filter->getEncounterSlots())std::cout<<b;std::cout<<std::endl;
        check(ivCount==6 && filter->getShiny()==3 && filter->getLevelMin()==20 && filter->getLevelMax()==20 && filter->getEncounterSlots()==encounterSlots,"exact IV/shiny/slot/level filters selected");
        std::cout<<"CACHE_STATUS "<<child<QLabel>(wild,"labelIVFastSearch")->text().toStdString()<<std::endl;
        json rows=json::array();
        const char *statNames[]={"HP","Atk","Def","SpA","SpD","Spe"};
        for(int policy=0;policy<2;++policy){
          SearchOptimization::setPruningEnabled(policy!=0);
          for(const auto &c:ivDomainCases()){
            for(size_t stat=0;stat<6;++stat){
                auto minimum=child<QSpinBox>(*filter,(std::string("spinBox")+statNames[stat]+"Min").c_str());
                auto maximum=child<QSpinBox>(*filter,(std::string("spinBox")+statNames[stat]+"Max").c_str());
                minimum->setValue(0);maximum->setValue(c.bounds.max[stat]);minimum->setValue(c.bounds.min[stat]);
            }
            check(filter->getMinIVs()==c.bounds.min && filter->getMaxIVs()==c.bounds.max,"actual UI IV bounds");
            QElapsedTimer elapsed;elapsed.start();
            check(QMetaObject::invokeMethod(&wild,"search",Qt::DirectConnection),"actual form dispatch probe");
            auto row=IVRangeProbe::row;row["name"]=c.name;row["min"]=c.bounds.min;row["max"]=c.bounds.max;
            row["policy"]=policy==0?"Smart OFF":"Smart CPU";row["setup_ms"]=elapsed.elapsed();
            bool cache=row["fastSearchEnabled"].get<bool>();
            check(row["payload_guard"].get<bool>()==(policy!=0),"all legal intervals retain payload eligibility");
            check(row["searcher_class"]==(cache?"WildSearcher5CacheFast":"WildSearcher5"),"normal cache-fast priority");
            check(row["actual_payload_first"].get<bool>()==(!cache&&policy!=0),"ordinary Smart Search dispatch");
            row["backend"]=cache?"cache-fast":policy==0?"ordinary baseline":"smart CPU";
            rows.push_back(row);std::cout<<row.dump()<<std::endl;
          }
        }
        std::ofstream file(QDir(output).filePath("paths.json").toStdString());file<<rows.dump(2);
        wild.close();std::cout<<"PATH_PROBE_PASS no_search_workers_started"<<std::endl;return 0;
    }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
