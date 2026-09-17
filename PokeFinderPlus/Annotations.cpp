#include "PluginApi.hpp"
#include "Annotations/AnnotationWindow.hpp"

#include <QPointer>

namespace
{
QPointer<AnnotationWindow> window;
}

extern "C" __declspec(dllexport) unsigned int pokefinder_plus_plugin_api_version()
{
    return POKEFINDER_PLUS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char *pokefinder_plus_plugin_name()
{
    return "Annotations";
}

extern "C" __declspec(dllexport) bool pokefinder_plus_plugin_initialize(QWidget *)
{
    return true;
}

extern "C" __declspec(dllexport) void pokefinder_plus_plugin_shutdown()
{
    delete window.data();
    window = nullptr;
}

extern "C" __declspec(dllexport) void pokefinder_plus_plugin_open_window(QWidget *)
{
    if (!window)
    {
        window = new AnnotationWindow;
    }
    window->show();
    window->raise();
    window->activateWindow();
}
