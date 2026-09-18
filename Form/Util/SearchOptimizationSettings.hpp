#ifndef SEARCHOPTIMIZATIONSETTINGS_HPP
#define SEARCHOPTIMIZATIONSETTINGS_HPP

#include <Core/Util/SearchOptimization.hpp>
#include <QSettings>

namespace SearchOptimizationSettings
{
    // Shared by application startup and the Settings form. The caller has
    // entered QSettings' "settings" group. Missing family keys migrate to ON;
    // the existing master/GPU preferences and dependency are preserved.
    inline void load(QSettings &settings)
    {
        settings.remove("plusSearchWorkerLimit");
        for (size_t i = 0; i < SearchOptimization::FamilyCount; ++i)
        {
            const auto family = static_cast<SearchOptimization::Family>(i);
            const auto key = SearchOptimization::FamilyKeys[i];
            const bool enabled = settings.value(key, SearchOptimization::DefaultFamily).toBool();
            SearchOptimization::setFamilyEnabled(family, enabled);
            settings.setValue(key, enabled);
        }
        const bool smart = settings.value("plusSearchPruning", SearchOptimization::DefaultPruning).toBool();
        const bool gpu = smart && settings.value("plusSearchGpu", SearchOptimization::DefaultGpu).toBool();
        if (!smart) settings.setValue("plusSearchGpu", false);
        SearchOptimization::setPruningEnabled(smart);
        SearchOptimization::setGpuEnabled(gpu);
    }
}

#endif
