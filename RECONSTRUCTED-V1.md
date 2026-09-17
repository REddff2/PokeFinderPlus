# Reconstructed pre-Search-Optimization v1

This is a reconstruction from the current PokeFinder+ source, not an exact historical checkout and not a historical PokeFinder+ commit. The reference is clean Nick PokeFinder 5.1.5 at f1593751feba7e6cbc5daaf58488b12aaccfc24e; that hash identifies upstream only. About also identifies this build as reconstructed.

## Outputs

- Source: C:\PokeFinderDev\PokeFinderPlus-v1-src
- Separate Release build: C:\PokeFinderDev\build-v1
- Package: C:\PokeFinderDev\PokeFinderPlus-package\versions\v1\PokeFinder+
- ZIP: C:\PokeFinderDev\PokeFinderPlus-package\PokeFinderPlus-v1.zip
- ZIP SHA-256: 3a715b8b8ad9804fb737dc086c0ca6e38325edab1c837d5a4ac12f237478baea

Install PokeFinder+ inside a matching normal PokeFinder release using Qt 6.10.1 x64, then run PokeFinder+.exe. Parent runtime files are not included in the add-on or ZIP.

## Restored byte-for-byte from upstream

- Core/CMakeLists.txt
- Core/Gen5/Generators/EventGenerator5.cpp
- Core/Gen5/Generators/EventGenerator5.hpp
- Core/Gen5/Generators/WildGenerator5.cpp
- Core/Gen5/Generators/WildGenerator5.hpp
- Core/Gen5/Searchers/IVSearcher5.cpp
- Core/Gen5/Searchers/IVSearcher5.hpp
- Core/Gen5/Searchers/ProfileSearcher5.cpp
- Core/Parents/Filters/StateFilter.cpp
- Core/Parents/Filters/StateFilter.hpp
- Core/Parents/Searchers/SearcherBase.hpp
- Form/Util/Settings.cpp
- Form/Util/Settings.ui

## Mixed files selectively cleaned

- CMakeLists.txt: removed GPU helper and optimization verification targets; kept plugin host, launcher output, and branding integration.
- main.cpp: removed optimization initialization and smart/GPU settings persistence; kept branding and opt-in plugin diagnostics.
- Form/Gen5/Wild5.cpp: restored the upstream search call while retaining proxy-safe navigation.
- Form/CMakeLists.txt: replaced unavailable Git metadata with an explicit reconstruction label.

## PokeFinder+ infrastructure retained unchanged

- PokeFinderPlus/PluginApi.hpp
- PokeFinderPlus/PluginManager.cpp
- PokeFinderPlus/PluginManager.hpp
- PokeFinderPlus/PokeFinderPlusLauncher.cpp
- PokeFinderPlus/Branding.cpp
- PokeFinderPlus/Branding.hpp
- PokeFinderPlus/Branding.cmake
- PokeFinderPlus/branding.rc.in
- PokeFinderPlus/qt.conf
- PokeFinderPlus/resources/pokefinder-plus.ico
- PokeFinderPlus/resources/PokeFinderPlus-256.png
- Model/ModelIndexMapping.hpp

PluginManager retains discovery/loading, headless support with optional open_window export, menu enable/disable persistence, and opening the plugins directory. Source-only plugin implementations remain in the isolated copy; no plugin DLL is shipped.

Proxy-safe host navigation remains in:

- Form/Gen3/Static3.cpp
- Form/Gen3/Wild3.cpp
- Form/Gen4/Eggs4.cpp
- Form/Gen4/Event4.cpp
- Form/Gen4/PokeRadar.cpp
- Form/Gen4/Static4.cpp
- Form/Gen4/Wild4.cpp
- Form/Gen5/HiddenGrotto.cpp
- Form/Gen5/Phenomenon.cpp
- Form/Gen5/Static5.cpp
- Form/Gen5/Tools/AdjacentSeeds.cpp
- Form/Gen5/Wild5.cpp
- Form/Util/AdvanceFinder.cpp

## Removed

- Core/Gen5/GPU
- Core/Util/SearchOptimization.cpp
- Core/Util/SearchOptimization.hpp
- Core/Util/SearchMetrics.hpp
- Test/SearchOptimization

All associated Core optimization changes, CPU worker instrumentation, pruning, reduced MT, payload-first/bulk paths, OpenCL guards/fallback, settings, documentation, benchmarks and harnesses were removed or restored to upstream.

## Verification

- All 1264 Core files match upstream byte-for-byte, excluding Git metadata and Python caches. Settings.cpp and Settings.ui also match byte-for-byte.
- Remaining Form/Model differences are plugin/branding integration, generic proxy-safe navigation, and reconstruction metadata. Complete differences: build-v1/evidence/retained-host-differences.diff.
- Original upstream regression suite: 636 passes across 54 suites, zero failures and zero skipped. Coverage includes Gen 3/4 searches, Gen 5 Event/Wild and other generators, profile searching, RNG and utilities.
- Actual packaged app/launcher tested from a byte-identical copy inside an isolated copy of the normal matching release. Loaded modules came from parent Qt/qwindows.dll, with SDK paths removed from PATH.
- PokeFinder+ 5.1.5 title verified. Both EXEs embed all seven exact branded icon frames (16, 24, 32, 48, 64, 128, 256 px). The native 48 px window icon matches the source ICO pixel-for-pixel.
- Plugins contains only Plugin Manager...; activating it opens the exact empty plugins folder in Explorer.
- Settings has no Search Optimization, smart-search, CPU-budget or GPU controls. The upstream Threads control remains.
- About says Reconstructed pre-Search-Optimization v1 and None - reconstructed source.
- Normal Gen 3 Static tool opens. App exit 0; launcher exit 0.
- Headless plugin load/disable/re-enable/shutdown, plugin data paths, traversal rejection, stacked proxy mapping, and existing IV Total proxy/model tests all pass. Test fixtures remain under build-v1 only.
- Current source, clean upstream, FINAL and TEST are unchanged by SHA-256 inventory. Original Windows PokeFinder settings restored after UI testing; isolated profile data used.
- ZIP integrity and file hashes verified; empty directories explicitly included.

## Exact package and ZIP contents

    PokeFinder+/
      PokeFinder+.exe
      PokeFinderPlusApp.exe
      qt.conf
      plugins.ini        (0 bytes)
      plugins/           (empty)
      data/              (empty)

No DLLs, Qt runtime, plugin binaries, debug/test executables, logs, source, GPU helper, or kernel files are included. This report is outside the add-on and ZIP.

| File | Bytes | SHA-256 |
|---|---:|---|
| PokeFinder+.exe | 74752 | c419078c2c38b95b614d6b2855e035bd4843da4e18bec55948bd69e7cce33c6d |
| PokeFinderPlusApp.exe | 6605312 | 56644c7cbf79955d642644f18d63e4b36f97a2eba4fdf6a5ff08bf9e767e61e6 |
| plugins.ini | 0 | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| qt.conf | 28 | e176e2ac315a17cd0dbbb2f281aa9c533e31ffb4ff02cdf0f79693b9d92b1c44 |
