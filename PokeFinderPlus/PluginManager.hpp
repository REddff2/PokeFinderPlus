#ifndef POKEFINDERPLUS_PLUGINMANAGER_HPP
#define POKEFINDERPLUS_PLUGINMANAGER_HPP

#include <QObject>
#include <memory>
#include <vector>

class QMenu;
class QWidget;

class PluginManager final : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QWidget *host);
    ~PluginManager() override;

    QMenu *menu() const;

private:
    struct LoadedPlugin;

    bool eventFilter(QObject *watched, QEvent *event) override;
    void discoverPlugins();
    void addPluginAction(LoadedPlugin &plugin);
    void activatePlugin(LoadedPlugin &plugin);
    void setPluginEnabled(LoadedPlugin &plugin, bool enabled);
    void syncPluginWindows(LoadedPlugin &plugin);
    void openPluginsFolder();
    void saveEnabledState(const LoadedPlugin &plugin, bool enabled);

    QWidget *host;
    QMenu *pluginsMenu;
    std::vector<std::unique_ptr<LoadedPlugin>> plugins;
    bool shuttingDown = false;
};

#endif
