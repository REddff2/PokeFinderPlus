#include "NetworkProbe.hpp"
#include <PokeFinderPlus/PluginApi.hpp>
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTextStream>
#include <QTimer>
namespace
{
QString mode,root;
int setups=0,errors=0,ticks=0;
struct Observer : QObject
{
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if(event->type()==QEvent::Show)
        {
            if(object->objectName()=="SlotSpritesAssetSetup")
            {
                ++setups;
                if(mode=="cancel") QTimer::singleShot(10,object,[object]{QMetaObject::invokeMethod(object,"canceled",Qt::DirectConnection);});
            }
            if(object->objectName()=="SlotSpritesAssetError") ++errors;
        }
        return false;
    }
};
Observer *observer=nullptr;
void report(bool passed)
{
    QFile file(root+"/verification/network-"+mode+".log");file.open(QIODevice::WriteOnly);
    QTextStream out(&file);
    out<<(passed?"PASS ":"FAIL ")<<mode<<" WinHTTP sessions="<<NetworkProbe::calls.load()
       <<" networkOnGui="<<NetworkProbe::onGui.load()<<" setupWindows="<<setups<<" errors="<<errors<<" GUI ticks="<<ticks<<'\n';
}
}
extern "C" __declspec(dllexport) unsigned int pokefinder_plus_plugin_api_version(){return POKEFINDER_PLUS_PLUGIN_API_VERSION;}
extern "C" __declspec(dllexport) const char *pokefinder_plus_plugin_name(){return "Slot Sprites TEST network probe";}
extern "C" __declspec(dllexport) bool pokefinder_plus_plugin_initialize(QWidget *host)
{
    mode=qEnvironmentVariable("SLOT_SPRITES_NETWORK_TEST");if(mode.isEmpty())return false;
    root=PokeFinderPlus::pluginDataDirectory("SlotSprites");QDir().mkpath(root+"/verification");
    NetworkProbe::offline=mode=="reuse" || mode=="missing";
    if(!NetworkProbe::install(GetModuleHandleW(L"SlotSprites.dll")))return false;
    observer=new Observer; qApp->installEventFilter(observer);
    auto *timer=new QTimer(observer);
    QObject::connect(timer,&QTimer::timeout,observer,[host]{
        ++ticks;
        if(mode!="missing" && mode!="cancel")return;
        bool setup=false;
        for(auto *w:QApplication::topLevelWidgets()) if(w->objectName()=="SlotSpritesAssetSetup")setup=true;
        if(!setups || setup || (mode=="missing" && !errors))return;
        QAction *action=nullptr;
        for(auto *candidate:host->findChildren<QAction *>())if(candidate->text()=="Slot Sprites")action=candidate;
        const bool passed=action && !action->isChecked() && !NetworkProbe::onGui
            && !QFileInfo::exists(root+"/download-temp") && errors==(mode=="missing"?1:0);
        report(passed);QCoreApplication::exit(passed?0:1);
    });timer->start(5);
    QObject::connect(qApp,&QCoreApplication::aboutToQuit,observer,[]{
        if(mode=="online" || mode=="reuse")report(!NetworkProbe::onGui && errors==0
            && NetworkProbe::calls==(mode=="online"?1:0) && setups==(mode=="online"?1:0));
    });
    QTimer::singleShot(240000,observer,[]{report(false);QCoreApplication::exit(2);});
    return true;
}
extern "C" __declspec(dllexport) void pokefinder_plus_plugin_shutdown(){delete observer;observer=nullptr;}
