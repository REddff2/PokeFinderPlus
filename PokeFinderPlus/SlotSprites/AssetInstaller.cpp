#include "AssetInstaller.hpp"
#include <PokeFinderPlus/PluginApi.hpp>
#include "WinHttpDownload.hpp"
#include "miniz.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

namespace
{
std::atomic_bool canceled{false};
bool settingUp = false;
void require(bool ok,const char *reason) { if(!ok) throw std::runtime_error(reason); }
QByteArray read(const QString &path)
{
    QFile file(path); if(!file.open(QIODevice::ReadOnly)) return {};
    if(file.size()>32*1024*1024) return {};
    return file.readAll();
}
QByteArray digest(const QByteArray &data) { return QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex(); }
void write(const QString &path,const QByteArray &bytes)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()),"Could not create sprite folders.");
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit(),"Could not save sprite data.");
}
bool plainPath(const QString &path)
{
    QFileInfo info(path);
    if(info.isSymLink() || info.isJunction()) return false;
    if(info.exists() && info.isDir())
    {
        QDirIterator it(path,QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDirIterator::Subdirectories);
        while(it.hasNext()) { it.next(); if(it.fileInfo().isSymLink() || it.fileInfo().isJunction()) return false; }
    }
    return true;
}
void cleanTemp(const QString &root)
{
    const QString temp=QDir(root).filePath("download-temp");
    require(QDir::isAbsolutePath(root) && QFileInfo(root).fileName()=="SlotSprites" && plainPath(temp),"Unsafe temporary sprite path.");
    require(!QFileInfo::exists(temp+"/RECOVERY_REQUIRED.txt"),"Previous sprite files need recovery; see download-temp/RECOVERY_REQUIRED.txt.");
    if(QFileInfo::exists(temp)) require(QDir(temp).removeRecursively(),"Could not clear temporary sprite files.");
}
struct Recipe
{
    QJsonObject json,files;
    QByteArray manifest,attribution;
    Recipe()
    {
        json=QJsonDocument::fromJson(read(":/slotsprites/bootstrap.json")).object();
        files=json.value("files").toObject();
        manifest=json.value("manifestText").toString().toUtf8();
        attribution=json.value("attributionText").toString().toUtf8();
        require(json.value("layout").toInt()==2 && !files.isEmpty() && !manifest.isEmpty(),"Invalid embedded sprite recipe.");
    }
};
bool complete(const QString &root,const Recipe &recipe,std::atomic_int &count)
{
    if(!plainPath(root) || read(root+"/manifest.json")!=recipe.manifest) return false;
    for(auto it=recipe.files.begin();it!=recipe.files.end();++it)
    {
        if(canceled) return false;
        const auto expected=it.value().toObject();
        QFileInfo file(root+'/'+it.key());
        if(!file.isFile() || file.size()!=expected.value("size").toInteger()) return false;
        if(digest(read(file.absoluteFilePath()))!=expected.value("sha256").toString().toLatin1()) return false;
        ++count;
    }
    for(const auto &name:{"pokemon.json","item-map.json"})
        if(!QJsonDocument::fromJson(read(root+"/data/"+name)).isObject()) return false;
    return true;
}
void marker(const QString &root,const Recipe &recipe)
{
    QJsonObject record{{"source","https://github.com/msikma/pokesprite"},{"commit",recipe.json.value("commit")},
                       {"layoutVersion",2},{"installedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                       {"fileCount",recipe.files.size()}};
    write(root+"/installed.json",QJsonDocument(record).toJson());
}
// Work on files/ZIP on a worker thread. The local event loop services the GUI;
// it bridges the frozen synchronous bool plugin API without declaring success early.
bool work(const std::function<bool()> &operation,QProgressDialog *progress=nullptr,std::atomic_int *count=nullptr,int total=0)
{
    bool result=false; std::exception_ptr error;
    QEventLoop loop;
    auto thread=std::unique_ptr<QThread>(QThread::create([&]{try {result=operation();} catch(...) {error=std::current_exception();}}));
    QObject::connect(thread.get(),&QThread::finished,&loop,&QEventLoop::quit);
    QTimer updates;
    if(progress && count)
    {
        progress->setRange(0,total); progress->setValue(0);
        QObject::connect(&updates,&QTimer::timeout,&loop,[&]{const int value=count->load(); if(value<0) progress->setRange(0,0); else {progress->setRange(0,total);progress->setValue(value);}}); updates.start(40);
    }
    thread->start(); loop.exec();
    if(thread->isRunning()) canceled=true;
    thread->wait();
    if(error) std::rethrow_exception(error);
    return result && !canceled;
}

void extractAndPublish(const QString &root,const Recipe &recipe,std::atomic_int &count)
{
    const QString stage=root+"/download-temp/staging";
    const auto archive=read(root+"/download-temp/pokesprite.zip");
    mz_zip_archive zip{};
    require(mz_zip_reader_init_mem(&zip,archive.constData(),size_t(archive.size()),0),"The sprite archive could not be read.");
    struct CloseZip {mz_zip_archive *zip; ~CloseZip(){mz_zip_reader_end(zip);}} closeZip{&zip};
    QHash<QString,mz_uint> entries;
    for(mz_uint n=0;n<mz_zip_reader_get_num_files(&zip);++n)
    {
        mz_zip_archive_file_stat stat{};
        require(mz_zip_reader_file_stat(&zip,n,&stat),"Invalid ZIP directory.");
        const QString name=QString::fromUtf8(stat.m_filename);
        require(!entries.contains(name),"Duplicate archive entry."); entries.insert(name,n);
    }
    const QString prefix="pokesprite-"+recipe.json.value("commit").toString()+'/';
    for(auto it=recipe.files.begin();it!=recipe.files.end();++it)
    {
        require(!canceled,"Sprite setup canceled.");
        const auto expected=it.value().toObject();
        const QString path=prefix+expected.value("archive").toString();
        require(entries.contains(path),"Sprite archive is incomplete.");
        mz_zip_archive_file_stat entry{};
        require(mz_zip_reader_file_stat(&zip,entries.value(path),&entry),"Invalid sprite archive entry.");
        require(!entry.m_is_directory && !entry.m_is_encrypted
                && ((entry.m_external_attr>>16)&0170000)!=0120000
                && entry.m_uncomp_size==quint64(expected.value("size").toInteger())
                && entry.m_uncomp_size<2*1024*1024,"Sprite archive entry is invalid.");
        QByteArray bytes(qsizetype(entry.m_uncomp_size),Qt::Uninitialized);
        require(mz_zip_reader_extract_to_mem(&zip,entries.value(path),bytes.data(),size_t(bytes.size()),0),"Could not decompress sprite entry.");
        require(digest(bytes)==expected.value("sha256").toString().toLatin1(),"Sprite archive validation failed.");
        write(stage+'/'+it.key(),bytes); ++count;
    }
    write(stage+"/manifest.json",recipe.manifest);
    write(stage+"/POKESPRITE_ATTRIBUTION.txt",recipe.attribution);
    write(stage+"/MINIZ-LICENSE.txt",read(":/slotsprites/MINIZ-LICENSE.txt"));
    std::atomic_int validated{0};
    require(complete(stage,recipe,validated),"Staged sprite data is incomplete.");
    require(!canceled,"Sprite setup canceled.");
    const QString previous=root+"/download-temp/previous"; require(QDir().mkpath(previous),"Could not stage installation backup.");
    QStringList saved,published;
    const QStringList names{"sprites","data","manifest.json","POKESPRITE-LICENSE.md","POKESPRITE_ATTRIBUTION.txt","MINIZ-LICENSE.txt"};
    const auto oldMarker=read(root+"/installed.json");
    try
    {
        if(QFileInfo::exists(root+"/installed.json")) require(QFile::remove(root+"/installed.json"),"Could not reset installation marker.");
        for(const auto &name:names)
        {
            require(plainPath(root+'/'+name),"Unsafe destination path.");
            if(QFileInfo::exists(root+'/'+name)) {require(QDir().rename(root+'/'+name,previous+'/'+name),"Could not back up existing sprite data.");saved.append(name);}
            require(QDir().rename(stage+'/'+name,root+'/'+name),"Could not publish sprite data."); published.append(name);
        }
        validated=0; require(complete(root,recipe,validated),"Final sprite validation failed.");
        marker(root,recipe); // Commit last. No rendering controller exists until this succeeds.
    }
    catch(...)
    {
        bool restored=true;
        for(auto it=published.crbegin();it!=published.crend();++it) restored &= QDir().rename(root+'/'+*it,stage+'/'+*it);
        for(auto it=saved.crbegin();it!=saved.crend();++it) restored &= QDir().rename(previous+'/'+*it,root+'/'+*it);
        if(!restored)
        {
            // Even if a full disk prevents writing this notice, the outer failure
            // cleanup checks for remaining backup files and keeps them intact.
            try {write(root+"/download-temp/RECOVERY_REQUIRED.txt", "Automatic rollback failed. Previous files are preserved in previous/. Close PokeFinder+ before recovering or removing this temporary directory.\n");}
            catch(...) {}
        }
        if(restored && !oldMarker.isEmpty()) write(root+"/installed.json",oldMarker);
        throw;
    }
    cleanTemp(root);
}
}
namespace SlotSprites
{
void cancelAssetSetup() {canceled=true;}
bool ensureAssets(QWidget *host)
{
    if(settingUp) return false;
    QScopedValueRollback guard(settingUp,true); canceled=false;
    QPointer<QWidget> safeHost(host); bool quitting=false;
    QObject context;
    QObject::connect(qApp,&QCoreApplication::aboutToQuit,&context,[&]{quitting=true;canceled=true;});
    if(host) QObject::connect(host,&QObject::destroyed,&context,[&]{quitting=true;canceled=true;});
    QPointer<QAction> action;
    if(host) for(auto *candidate:host->findChildren<QAction *>()) if(candidate->text()=="Slot Sprites") {action=candidate;break;}
    const bool actionEnabled=action && action->isEnabled(); if(action) action->setEnabled(false);
    struct RestoreAction {QPointer<QAction> action;bool enabled;~RestoreAction(){if(action) action->setEnabled(enabled);}} restore{action,actionEnabled};
    const QString root=PokeFinderPlus::pluginDataDirectory("SlotSprites");
    try
    {
        require(!root.isEmpty() && QDir::isAbsolutePath(root) && plainPath(root) && plainPath(QFileInfo(root).absolutePath()),"Unsafe sprite data directory.");
        Recipe recipe; std::atomic_int count{0};
        if(work([&]{return complete(root,recipe,count);}))
        {
            if(read(root+"/POKESPRITE_ATTRIBUTION.txt")!=recipe.attribution) write(root+"/POKESPRITE_ATTRIBUTION.txt",recipe.attribution);
            if(read(root+"/MINIZ-LICENSE.txt")!=read(":/slotsprites/MINIZ-LICENSE.txt")) write(root+"/MINIZ-LICENSE.txt",read(":/slotsprites/MINIZ-LICENSE.txt"));
            if(!QFileInfo::exists(root+"/installed.json")) marker(root,recipe);
            return true; // No WinHTTP session or setup window is constructed on this path.
        }
        if(canceled) return false;
        require(QDir().mkpath(root),"Could not create the sprite data directory.");
        QLockFile lock(root+"/install.lock");
        require(lock.tryLock(0),"Sprite setup is already running in another PokeFinder+ instance.");
        count=0; if(work([&]{return complete(root,recipe,count);})) return true;
        cleanTemp(root); require(QDir().mkpath(root+"/download-temp"),"Could not create temporary sprite directory.");
        QProgressDialog progress("Downloading sprite assets...","Cancel",0,100,nullptr);
        progress.setObjectName("SlotSpritesAssetSetup"); progress.setWindowTitle("Slot Sprites");
        progress.setWindowModality(Qt::NonModal); progress.setAutoClose(false); progress.setAutoReset(false); progress.setMinimumDuration(0);
        QObject::connect(&progress,&QProgressDialog::canceled,&context,[]{canceled=true;});
        progress.show();
        try
        {
            count=0;
            require(work([&]{SlotSprites::downloadArchive(recipe.json.value("archiveUrl").toString(),root+"/download-temp/pokesprite.zip",canceled,count);return true;},&progress,&count,100),"Sprite setup canceled.");
            progress.setLabelText("Installing and validating sprite assets..."); count=0;
            require(work([&]{extractAndPublish(root,recipe,count);return true;},&progress,&count,recipe.files.size()),"Sprite setup canceled.");
        }
        catch(...)
        {
            // QProgressDialog::closeEvent emits canceled, including a programmatic
            // close. Disconnect first so a network error is not mistaken for Cancel.
            QObject::disconnect(&progress,nullptr,&context,nullptr); progress.close();
            const bool backupRemains=!QDir(root+"/download-temp/previous").entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System).isEmpty();
            if(!backupRemains && plainPath(root+"/download-temp") && !QFileInfo::exists(root+"/download-temp/RECOVERY_REQUIRED.txt")) cleanTemp(root);
            throw;
        }
        QObject::disconnect(&progress,nullptr,&context,nullptr); progress.close(); return true;
    }
    catch(const std::exception &error)
    {
        if(!quitting && !canceled)
        {
            auto *message=new QMessageBox(QMessageBox::Warning,"Slot Sprites",
                "Slot Sprites could not download or install its sprite data.\nCheck your internet connection and try enabling the plugin again.",
                QMessageBox::Ok,safeHost);
            message->setObjectName("SlotSpritesAssetError"); message->setDetailedText(QString::fromUtf8(error.what()));
            message->setAttribute(Qt::WA_DeleteOnClose); message->open();
        }
        return false;
    }
}
}
