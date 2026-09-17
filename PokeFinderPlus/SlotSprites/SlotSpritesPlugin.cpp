#include <PokeFinderPlus/PluginApi.hpp>
#include "SlotSpritesController.hpp"
#include "AssetInstaller.hpp"
#include <memory>

namespace { std::unique_ptr<SlotSpritesController> controller; }
extern "C" __declspec(dllexport) unsigned int pokefinder_plus_plugin_api_version() { return POKEFINDER_PLUS_PLUGIN_API_VERSION; }
extern "C" __declspec(dllexport) const char *pokefinder_plus_plugin_name() { return "Slot Sprites"; }
extern "C" __declspec(dllexport) bool pokefinder_plus_plugin_initialize(QWidget *host)
{
    if (!controller && !SlotSprites::ensureAssets(host)) return false;
    if (!controller) controller = std::make_unique<SlotSpritesController>();
    if (!controller->ready()) { controller.reset(); return false; }
    return true;
}
extern "C" __declspec(dllexport) void pokefinder_plus_plugin_shutdown() { SlotSprites::cancelAssetSetup(); controller.reset(); }
