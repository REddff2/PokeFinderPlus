#include "PluginManager.hpp"
#include "PluginApi.hpp"

#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfoList>
#include <QMenu>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QDesktopServices>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QSettings>
#include <QLibrary>
#include <QString>
#include <QTextStream>
#include <fstream>

namespace
{
QString applicationDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
}

void logMessage(const QString &message)
{
    QSettings diagnostics(QDir(applicationDirectory()).filePath("plugins.ini"), QSettings::IniFormat);
    if (!diagnostics.value("diagnostics/logging", false).toBool())
    {
        return;
    }
    std::ofstream logFile(QDir(applicationDirectory()).filePath("PokeFinderPlus.log").toStdString(), std::ios::app);
    logFile << QDateTime::currentDateTime().toString(Qt::ISODate).toStdString() << " " << message.toStdString() << '\n';
}

QString yesNo(bool value)
{
    return value ? "yes" : "no";
}
}

struct PluginManager::LoadedPlugin
{
    QString fileName;
    QString displayName;
    QLibrary library;
    PokeFinderPlusPluginInitialize initialize = nullptr;
    PokeFinderPlusPluginShutdown shutdown = nullptr;
    PokeFinderPlusPluginOpenWindow openWindow = nullptr;
    QAction *action = nullptr;
    QList<QPointer<QWidget>> windows;
    bool enabled = false;
    bool initialized = false;
};

PluginManager::PluginManager(QWidget *host) : QObject(host), host(host), pluginsMenu(new QMenu(tr("Plugins"), host))
{
    pluginsMenu->installEventFilter(this);
    discoverPlugins();

    QAction *managerAction = pluginsMenu->addAction(tr("Plugin Manager..."));
    connect(managerAction, &QAction::triggered, this, &PluginManager::openPluginsFolder);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { shuttingDown = true; });
    // Restore saved active windows once the host has finished starting up.
    QTimer::singleShot(0, this, [this] {
        for (const auto &plugin : plugins)
        {
            if (plugin->enabled)
                setPluginEnabled(*plugin, true);
        }
    });
}

PluginManager::~PluginManager()
{
    shuttingDown = true;
    for (const auto &plugin : plugins)
    {
        for (const auto &window : plugin->windows)
        {
            if (window)
            {
                window->removeEventFilter(this);
                disconnect(window, nullptr, this, nullptr);
            }
        }
        if (plugin->initialized && plugin->shutdown)
        {
            logMessage(QString("shutdown %1").arg(plugin->displayName));
            plugin->shutdown();
        }
    }
}

QMenu *PluginManager::menu() const
{
    return pluginsMenu;
}

bool PluginManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == pluginsMenu)
    {
        QAction *action = nullptr;
        if (event->type() == QEvent::MouseButtonRelease)
        {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                action = pluginsMenu->actionAt(mouseEvent->position().toPoint());
            }
        }
        else if (event->type() == QEvent::KeyPress)
        {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter)
            {
                action = pluginsMenu->activeAction();
            }
        }

        if (action && action->isEnabled())
        {
            for (const auto &plugin : plugins)
            {
                if (plugin->action == action)
                {
                    // Handle the logical toggle before QMenu can dismiss itself.
                    activatePlugin(*plugin);
                    event->accept();
                    return true;
                }
            }
        }
    }
    else if (!shuttingDown && event->type() == QEvent::Hide)
    {
        for (const auto &plugin : plugins)
        {
            for (const auto &window : plugin->windows)
            {
                if (window.data() == watched)
                {
                    syncPluginWindows(*plugin);
                    break;
                }
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void PluginManager::discoverPlugins()
{
    const QString appPath = QCoreApplication::applicationFilePath();
    const QString currentPath = QDir::currentPath();
    const QString pluginsPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("plugins");
    QDir pluginsDirectory(pluginsPath);
    logMessage(QString("applicationFilePath=%1").arg(appPath));
    logMessage(QString("applicationDirPath=%1").arg(applicationDirectory()));
    logMessage(QString("currentWorkingDirectory=%1").arg(currentPath));
    logMessage(QString("pluginsDirectory=%1 exists=%2").arg(pluginsDirectory.absolutePath(), yesNo(pluginsDirectory.exists())));
    pluginsDirectory.mkpath(".");

    QSettings settings(QDir(applicationDirectory()).filePath("plugins.ini"), QSettings::IniFormat);
    const QFileInfoList files = pluginsDirectory.entryInfoList({"*.dll"}, QDir::Files, QDir::Name);
    logMessage(QString("dllCount=%1").arg(files.size()));
    for (const QFileInfo &file : files)
    {
        logMessage(QString("foundDll=%1").arg(file.absoluteFilePath()));
        auto plugin = std::make_unique<LoadedPlugin>();
        plugin->fileName = file.fileName();
        plugin->library.setFileName(file.absoluteFilePath());
        plugin->library.setLoadHints(QLibrary::PreventUnloadHint);
        const bool loaded = plugin->library.load();
        logMessage(QString("load %1 success=%2").arg(file.absoluteFilePath(), yesNo(loaded)));
        if (!loaded)
        {
            logMessage(QString("loadError %1").arg(plugin->library.errorString()));
            continue;
        }

        auto apiVersion = reinterpret_cast<PokeFinderPlusPluginApiVersion>(plugin->library.resolve("pokefinder_plus_plugin_api_version"));
        auto name = reinterpret_cast<PokeFinderPlusPluginName>(plugin->library.resolve("pokefinder_plus_plugin_name"));
        plugin->initialize = reinterpret_cast<PokeFinderPlusPluginInitialize>(plugin->library.resolve("pokefinder_plus_plugin_initialize"));
        plugin->shutdown = reinterpret_cast<PokeFinderPlusPluginShutdown>(plugin->library.resolve("pokefinder_plus_plugin_shutdown"));
        plugin->openWindow = reinterpret_cast<PokeFinderPlusPluginOpenWindow>(plugin->library.resolve("pokefinder_plus_plugin_open_window"));
        logMessage(QString("exports api_version=%1 name=%2 initialize=%3 shutdown=%4 open_window=%5")
                       .arg(yesNo(apiVersion != nullptr), yesNo(name != nullptr), yesNo(plugin->initialize != nullptr),
                            yesNo(plugin->shutdown != nullptr), yesNo(plugin->openWindow != nullptr)));
        if (!apiVersion || !name || !plugin->initialize || !plugin->shutdown)
        {
            logMessage("required export missing; plugin skipped");
            continue;
        }

        const unsigned int version = apiVersion();
        logMessage(QString("apiVersion=%1 expected=%2").arg(version).arg(POKEFINDER_PLUS_PLUGIN_API_VERSION));
        if (version != POKEFINDER_PLUS_PLUGIN_API_VERSION)
        {
            logMessage("API version mismatch; plugin skipped");
            continue;
        }

        plugin->displayName = QString::fromUtf8(name());
        plugin->enabled = settings.value(QString("plugins/%1/enabled").arg(plugin->fileName), true).toBool();
        logMessage(QString("plugin=%1 enabled=%2").arg(plugin->displayName, yesNo(plugin->enabled)));
        addPluginAction(*plugin);
        plugins.push_back(std::move(plugin));
    }
}

void PluginManager::addPluginAction(LoadedPlugin &plugin)
{
    plugin.action = pluginsMenu->addAction(plugin.displayName);
    plugin.action->setCheckable(true);
    plugin.action->setChecked(false);
    // Also cover QAction activation through accessibility or menu mnemonics.
    // Use logical state rather than QAction's already-toggled checkmark.
    connect(plugin.action, &QAction::triggered, this, [&plugin, this] {
        activatePlugin(plugin);
    });
}

void PluginManager::activatePlugin(LoadedPlugin &plugin)
{
    setPluginEnabled(plugin, !plugin.enabled);
}

void PluginManager::setPluginEnabled(LoadedPlugin &plugin, bool enabled)
{
    if (enabled)
    {
        const auto existing = QApplication::topLevelWidgets();
        const QSet<QWidget *> before(existing.begin(), existing.end());
        if (!plugin.initialized)
        {
            plugin.initialized = plugin.initialize(host);
            logMessage(QString("initialize %1 success=%2").arg(plugin.displayName, yesNo(plugin.initialized)));
        }
        if (plugin.initialized && plugin.openWindow)
        {
            plugin.openWindow(host);
        }
        // The existing API creates its windows synchronously. Track only windows
        // created by this plugin's initialize/open calls, without changing the ABI.
        for (QWidget *window : QApplication::topLevelWidgets())
        {
            if (plugin.openWindow && window != host && !before.contains(window) && !qobject_cast<QMenu *>(window))
            {
                plugin.windows.append(window);
                window->installEventFilter(this);
                connect(window, &QObject::destroyed, this, [this, &plugin] { syncPluginWindows(plugin); });
            }
        }
        // Plugins without a window export stay active after successful initialization.
        enabled = plugin.initialized && !plugin.openWindow;
        for (const auto &window : plugin.windows)
        {
            if (window && window->isVisible())
                enabled = true;
        }
    }
    plugin.enabled = enabled;
    plugin.action->setChecked(enabled);
    saveEnabledState(plugin, enabled);
    if (!enabled)
    {
        // Disconnect before shutdown, which may synchronously destroy the UI.
        for (const auto &window : plugin.windows)
        {
            if (window)
            {
                window->removeEventFilter(this);
                disconnect(window, nullptr, this, nullptr);
            }
        }
        plugin.windows.clear();
        if (plugin.initialized)
        {
            logMessage(QString("shutdown %1 (disabled)").arg(plugin.displayName));
            plugin.initialized = false;
            plugin.shutdown();
        }
    }
}

void PluginManager::syncPluginWindows(LoadedPlugin &plugin)
{
    // Do not destroy a plugin window while it is handling its own hide/close.
    QTimer::singleShot(0, this, [this, &plugin] {
        if (shuttingDown || !host->isVisible() || !plugin.enabled)
            return;
        for (const auto &window : plugin.windows)
        {
            // Minimized windows remain logically visible in Qt.
            if (window && window->isVisible())
                return;
        }
        setPluginEnabled(plugin, false);
    });
}

void PluginManager::openPluginsFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(applicationDirectory()).filePath("plugins")));
}

void PluginManager::saveEnabledState(const LoadedPlugin &plugin, bool enabled)
{
    QSettings settings(QDir(applicationDirectory()).filePath("plugins.ini"), QSettings::IniFormat);
    settings.setValue(QString("plugins/%1/enabled").arg(plugin.fileName), enabled);
    settings.sync();
}
