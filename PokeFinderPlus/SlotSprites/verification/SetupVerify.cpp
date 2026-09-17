#include "NetworkProbe.hpp"
#include <PokeFinderPlus/PluginApi.hpp>
#include <QApplication>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
struct Observer : QObject
{
    int setups=0,errors=0,ticks=0;
    bool cancel=false;
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if(event->type()==QEvent::Show)
        {
            if(object->objectName()=="SlotSpritesAssetSetup")
            {
                ++setups;
                if(cancel) QTimer::singleShot(10,object,[object]{
                    QMetaObject::invokeMethod(object,"canceled",Qt::DirectConnection);
                });
            }
            if(object->objectName()=="SlotSpritesAssetError") ++errors;
        }
        return false;
    }
};
int main(int argc,char **argv)
{
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    const QString mode=app.arguments().value(1,"reuse");
    NetworkProbe::offline=mode!="online" && mode!="cancel";
    QLibrary dll(QCoreApplication::applicationDirPath()+"/SlotSprites.dll");
    if(!dll.load() || !NetworkProbe::install(GetModuleHandleW(L"SlotSprites.dll"))) return 10;
    auto initialize=reinterpret_cast<PokeFinderPlusPluginInitialize>(dll.resolve("pokefinder_plus_plugin_initialize"));
    auto shutdown=reinterpret_cast<PokeFinderPlusPluginShutdown>(dll.resolve("pokefinder_plus_plugin_shutdown"));
    Observer observer; observer.cancel=mode=="cancel"; app.installEventFilter(&observer);
    QTimer heartbeat; QObject::connect(&heartbeat,&QTimer::timeout,&app,[&]{++observer.ticks;}); heartbeat.start(5);
    QWidget host; host.show();
    QTimer::singleShot(0,&app,[&]{
        QElapsedTimer elapsed; elapsed.start();
        const bool ready=initialize(&host);
        const bool expected=mode=="online" || mode=="reuse";
        const QString root=PokeFinderPlus::pluginDataDirectory("SlotSprites");
        bool passed=ready==expected && !NetworkProbe::onGui && !QFileInfo::exists(root+"/download-temp");
        if(mode=="reuse") passed &= NetworkProbe::calls==0 && observer.setups==0 && observer.errors==0;
        if(mode=="online") passed &= NetworkProbe::calls==1 && observer.setups==1 && observer.errors==0 && observer.ticks>1;
        if(mode=="missing") passed &= NetworkProbe::calls==1 && observer.setups==1 && observer.errors==1;
        if(mode=="cancel") passed &= observer.setups==1 && observer.errors==0 && observer.ticks>0;
        if(expected) passed &= QFileInfo::exists(root+"/sprites/items/berry/oran.png") && QFileInfo::exists(root+"/installed.json");
        shutdown();
        std::cout<<(passed?"PASS ":"FAIL ")<<mode.toStdString()<<" ready="<<ready<<" network="<<NetworkProbe::calls
            <<" guiNetwork="<<NetworkProbe::onGui<<" setups="<<observer.setups<<" errors="<<observer.errors
            <<" GUI ticks="<<observer.ticks<<" ms="<<elapsed.elapsed()<<std::endl;
        app.exit(passed?0:1);
    });
    return app.exec();
}
