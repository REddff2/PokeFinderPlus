#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Gen5/Static5.hpp>
#include <Form/Gen5/Eggs5.hpp>
#include <Form/Gen5/DreamRadar.hpp>
#include <Form/Gen5/HiddenGrotto.hpp>
#include <Form/Gen5/Pickup.hpp>
#include <Form/Controls/EggSettings.hpp>
#include <Form/Controls/Filter.hpp>
#include <Form/Util/Settings.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
static void check(bool pass,const char *message){if(!pass)throw std::runtime_error(message);std::cout<<"PASS "<<message<<std::endl;}
template<class T>static T* child(QObject&owner,const char*name){auto found=owner.findChild<T*>(name);if(!found)throw std::runtime_error(name);return found;}
static QStringList rows(QTableView*view){QStringList result;auto*m=view->model();for(int r=0;r<m->rowCount();++r){QStringList values;for(int c=0;c<m->columnCount();++c)values.push_back(m->index(r,c).data().toString());result.push_back(values.join('|'));}return result;}
static void inputs(QWidget &form){for(auto *edit:form.findChildren<QLineEdit*>()){auto name=edit->objectName();if(!name.contains("Generator"))continue;if(name.contains("Seed"))edit->setText("0123456789ABCDEF");else if(name.contains("IVAdvances")||name.contains("InitialAdvances")||name.endsWith("Offset"))edit->setText("0");else if(name.contains("MaxAdvances"))edit->setText("31");}form.setAttribute(Qt::WA_DeleteOnClose,false);form.show();QTest::qWait(30);}
int main(int argc,char**argv)
{
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);app.setOrganizationName("Gen5CoverageUI");app.setApplicationName("Gen5CoverageUI");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");app.setFont(QFont("Segoe UI",9));app.setStyle("fusion");
    try
    {
        check(argc==2,"output path supplied");QTemporaryDir temp(QDir(argv[1]).filePath("UiStore-XXXXXX"));check(temp.isValid(),"temporary profile/settings store");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());QFile file(temp.filePath("profiles.json"));check(file.open(QIODevice::WriteOnly),"profile fixture created");file.write("{}");file.close();ProfileLoader::init(temp.filePath("profiles.json").toStdWString());QSettings().setValue("settings/threads",4);QSettings().setValue("settings/locale","en");Q_INIT_RESOURCE(resources);Translator::init("en");
        QTimer watchdog;QObject::connect(&watchdog,&QTimer::timeout,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*box=qobject_cast<QMessageBox*>(w);box&&box->isVisible()){std::cerr<<"Unexpected validation dialog: "<<box->text().toStdString()<<std::endl;std::exit(2);}});watchdog.start(200);
        Settings settings;settings.setAttribute(Qt::WA_DeleteOnClose,false);auto*smart=child<QCheckBox>(settings,"checkBoxSearchPruning");auto*gpu=child<QCheckBox>(settings,"checkBoxSearchGpu");check(!smart->isChecked()&&!gpu->isChecked(),"OFF/OFF defaults retained");
        auto mode=[&](int m){smart->setChecked(m!=0);gpu->setChecked(m==2);check(SearchOptimization::pruningEnabled()==(m!=0)&&SearchOptimization::gpuEnabled()==(m==2),"native policy controls updated");};
        auto load=[&](Game game){ProfileLoader5::setProfiles({Profile5("UI coverage",game,12345,54321,"","",0x001122334455ULL,{true,false,false,false,false,false,false,false,false},0x60,6,6,false,0x1100,0x1100,false,false,DSType::DS,Language::English)});};
        auto generate=[&](QWidget&form,const char*method,const char*table,const char*name){QStringList baseline;for(int m=0;m<3;++m){mode(m);check(QMetaObject::invokeMethod(&form,method,Qt::DirectConnection),"real form generation invoked");auto actual=rows(child<QTableView>(form,table));check(!actual.empty(),"nonempty UI generation");if(!m)baseline=actual;else check(actual==baseline,"UI count/fields/order/multiplicity identical");}form.grab().save(QDir(argv[1]).filePath(QString(name)+".png"));std::cout<<"UI_GENERATOR_PASS "<<name<<" rows="<<baseline.size()<<std::endl;};
        load(Game::Black2);
        {Static5 form;inputs(form);generate(form,"generate","tableViewGenerator","Static");
            for(const char*name:{"textBoxSearcherInitialIVAdvances","textBoxSearcherMaxIVAdvances","textBoxSearcherInitialAdvances","textBoxSearcherMaxAdvances"})child<QLineEdit>(form,name)->setText("0");
            for(const char*name:{"dateEditSearcherStartDate","dateEditSearcherEndDate"})child<QDateEdit>(form,name)->setDate(QDate(2026,6,15));
            auto*filter=child<Filter>(form,"filterSearcher");for(auto*spin:filter->findChildren<QSpinBox*>())if(spin->maximum()==31&&spin->objectName().endsWith("Min"))spin->setValue(15);
            auto*button=child<QPushButton>(form,"pushButtonSearch");QStringList baseline;
            for(int m=0;m<3;++m){mode(m);QElapsedTimer timer;timer.start();button->click();while(!button->isEnabled()&&timer.elapsed()<30000)QTest::qWait(20);check(button->isEnabled()&&child<QProgressBar>(form,"progressBar")->value()==100,"real Static UI date search completed");auto actual=rows(child<QTableView>(form,"tableViewSearcher"));check(!actual.empty(),"UI date search has positives");if(!m)baseline=actual;else check(actual==baseline,"UI date search exact equality");}
            button->click();child<QPushButton>(form,"pushButtonCancel")->click();for(int i=0;i<200&&!button->isEnabled();++i)QTest::qWait(20);check(button->isEnabled(),"UI search cancellation completes");form.close();}
        for(Game game:{Game::Black,Game::Black2}){load(game);Eggs5 form;inputs(form);auto*egg=child<EggSettings>(form,"eggSettingsGenerator");child<QComboBox>(*egg,"comboBoxParentAGender")->setCurrentIndex(0);child<QComboBox>(*egg,"comboBoxParentBGender")->setCurrentIndex(1);check(egg->isValid(),"valid Egg parents");generate(form,"generate","tableViewGenerator",game==Game::Black?"Egg-BW":"Egg-B2W2");form.close();}
        load(Game::Black2);
        {DreamRadar form;inputs(form);child<QComboBox>(form,"comboBoxGeneratorSpecie1")->setCurrentIndex(1);generate(form,"generate","tableViewGenerator","Dream-Radar");form.close();}
        {HiddenGrotto form;inputs(form);generate(form,"pokemonGenerate","tableViewPokemonGenerator","Grotto-Pokemon");generate(form,"grottoGenerate","tableViewGrottoGenerator","Grotto-slots");form.close();}
        {Pickup form;inputs(form);for(auto*box:form.findChildren<QCheckBox*>())if(box->objectName().startsWith("checkBoxGeneratorSlot"))box->setChecked(true);generate(form,"generate","tableViewGenerator","Pickup");form.close();}
        mode(0);check(!QSettings().value("settings/plusSearchPruning").toBool()&&!QSettings().value("settings/plusSearchGpu").toBool(),"settings persist OFF/OFF");std::cout<<"GEN5_UI_PASS\n";return 0;
    }catch(const std::exception&e){std::cerr<<"GEN5_UI_FAIL "<<e.what()<<std::endl;return 1;}
}
