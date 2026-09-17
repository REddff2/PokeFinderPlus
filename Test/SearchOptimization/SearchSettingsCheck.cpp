#include <Form/Util/Settings.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QGroupBox>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);std::cout<<"PASS "<<message<<std::endl;}
int main(int argc,char**argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("SearchSettingsVerification");app.setApplicationName("SearchSettingsVerification");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");app.setFont(QFont("Segoe UI",9));app.setStyle("fusion");
    try{
        check(argc>=2,"output directory argument");QString output=QString::fromLocal8Bit(argv[1]);
        if(argc==2){
            QTemporaryDir temporary(QDir(output).filePath("SettingsMatrix-XXXXXX"));check(temporary.isValid(),"isolated store");
            for(const char*mode:{"clean","read-off","cpu","read-cpu","disable","read-off"}){
                QProcess child;child.start(app.applicationFilePath(),{output,temporary.path(),mode});
                check(child.waitForStarted(5000)&&child.waitForFinished(15000),"settings child completed");
                std::cout<<child.readAllStandardOutput().toStdString();std::cerr<<child.readAllStandardError().toStdString();
                check(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0,"separate-process persistence check");
            }
            std::cout<<"SETTINGS_RESTART_MATRIX_PASS"<<std::endl;return 0;
        }
        check(argc==4,"child arguments");const QString mode=argv[3];
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,argv[2]);
        QSettings saved;
        if(mode=="clean") {saved.clear();saved.setValue("settings/locale","en");saved.setValue("settings/threads",QThread::idealThreadCount());saved.setValue("settings/plusSearchWorkerLimit",true);}
        Settings settings;settings.setAttribute(Qt::WA_DeleteOnClose,false);settings.show();QTest::qWait(80);
        auto smart=settings.findChild<QCheckBox*>("checkBoxSearchPruning");
        auto group=settings.findChild<QGroupBox*>("groupBoxSearchOptimization");
        auto threads=settings.findChild<QComboBox*>("comboBoxThreads");
        check(group&&group->title()=="Gen 5 Smart Search"&&group->findChildren<QCheckBox*>().size()==1,"exact native Smart Search section");
        check(smart&&smart->text()=="Enable Gen 5 smart search optimizations","Smart Search label");
        check(smart->toolTip()=="Uses verified Gen 5 search optimizations where supported. Unsupported searches use normal PokeFinder behavior.","smart tooltip");
        check(!saved.contains("settings/plusSearchWorkerLimit"),"retired worker setting removed");
        check(threads&&threads->count()==QThread::idealThreadCount()&&threads->currentData().toInt()==QThread::idealThreadCount(),"normal Threads unchanged");
        if(mode=="cpu")smart->setChecked(true);
        if(mode=="disable") {check(smart->isChecked(),"start with Smart Search on");smart->setChecked(false);}
        bool wantSmart=mode=="cpu"||mode=="read-cpu";
        check(smart->isChecked()==wantSmart,"settings matrix checkbox state");
        check(SearchOptimization::pruningEnabled()==wantSmart,"settings matrix runtime policy");
        check(saved.value("settings/plusSearchPruning",false).toBool()==wantSmart,"saved Smart Search state");
        if(mode=="clean"||mode=="cpu")settings.grab().save(QDir(output).filePath("settings-"+mode+".png"));
        saved.sync();settings.close();std::cout<<"CHILD_PASS "<<mode.toStdString()<<std::endl;return 0;
    }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
