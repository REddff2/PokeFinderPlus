#ifndef POKEFINDERPLUS_PLUGINAPI_HPP
#define POKEFINDERPLUS_PLUGINAPI_HPP

#include <QtWidgets/QWidget>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#define POKEFINDER_PLUS_PLUGIN_API_VERSION 1u

namespace PokeFinderPlus
{
// Official plugin storage root, beside PokeFinderPlusApp.exe, never relative
// to the working directory or the parent PokeFinder runtime. Resolving paths
// does not create directories. See PLUGIN_DATA.md for write-time usage.
inline QString pluginDataRoot()
{
    if (!QCoreApplication::instance()) return {};
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    if (applicationDirectory.isEmpty()) return {};
    return QDir(applicationDirectory).filePath(QStringLiteral("data"));
}

// A stable ID is one ASCII component containing letters, digits, '_' or '-'.
// Reject paths/traversal instead of allowing them to escape the data root.
// Empty means invalid ID or unavailable application directory; never use it
// as a relative path. Missing directories are normal and are not errors.
inline QString pluginDataDirectory(const QString &pluginId)
{
    if (pluginId.isEmpty()) return {};
    for (QChar character : pluginId)
    {
        const ushort value = character.unicode();
        if (!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z')
              || (value >= '0' && value <= '9') || value == '_' || value == '-'))
            return {};
    }
    const QString root = pluginDataRoot();
    return root.isEmpty() ? QString() : QDir(root).filePath(pluginId);
}
}

#if defined(_WIN32)
#define POKEFINDER_PLUS_PLUGIN_EXPORT __declspec(dllimport)
#else
#define POKEFINDER_PLUS_PLUGIN_EXPORT
#endif

extern "C"
{
    using PokeFinderPlusPluginApiVersion = unsigned int (*)();
    using PokeFinderPlusPluginName = const char *(*)();
    using PokeFinderPlusPluginInitialize = bool (*)(QWidget *host);
    using PokeFinderPlusPluginShutdown = void (*)();
    using PokeFinderPlusPluginOpenWindow = void (*)(QWidget *host);
}

#endif
