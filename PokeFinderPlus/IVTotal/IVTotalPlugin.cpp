#include "../PluginApi.hpp"
#include "IVTotalController.hpp"
#include <memory>

namespace { std::unique_ptr<IVTotalController> controller; }
extern "C" __declspec(dllexport) unsigned int pokefinder_plus_plugin_api_version() { return POKEFINDER_PLUS_PLUGIN_API_VERSION; }
extern "C" __declspec(dllexport) const char *pokefinder_plus_plugin_name() { return "IV Total"; }
extern "C" __declspec(dllexport) bool pokefinder_plus_plugin_initialize(QWidget *)
{
    if (!controller) controller = std::make_unique<IVTotalController>();
    return true;
}
extern "C" __declspec(dllexport) void pokefinder_plus_plugin_shutdown() { controller.reset(); }
