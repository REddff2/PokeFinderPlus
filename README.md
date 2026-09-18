# PokeFinder+

PokeFinder+ is an add-on/fork of PokeFinder with plugin support and additional features.

## Download

For normal use, download the latest release ZIP from the [GitHub Releases page](https://github.com/REddff2/PokeFinderPlus/releases). You do not need the source code.

Do not use "Download ZIP" from the green Code button unless you want the source code.

Release package installation:

1. Download the PokeFinder+ release ZIP.
2. Extract the PokeFinder+ folder.
3. Place that folder inside your normal PokeFinder release folder.
4. Run `PokeFinder+\PokeFinder+.exe`.

Example:

```text
PokeFinder\
├─ PokeFinder.exe
├─ Qt/runtime files...
└─ PokeFinder+\
   ├─ PokeFinder+.exe
   ├─ PokeFinderPlusApp.exe
   ├─ PokeFinderGpuHelper.exe
   ├─ Gen5Wild.cl
   ├─ qt.conf
   ├─ plugins.ini
   ├─ plugins\
   └─ data\
```

## Plugins

Plugins are optional DLL files placed in `PokeFinder+\plugins\`.

No plugins are included with the base PokeFinder+ download.

## Gen 5 Smart Search

Enable Gen 5 Smart Search in Tools > Settings for the verified Wild, Static, Event, Eggs, Dream Radar, Hidden Grotto, Pickup, Adjacent Seeds, and Cache Builders optimizations. Smart Search is **OFF by default**, and the normal PokeFinder Threads setting is unchanged.

When Smart Search is ON, **Advanced options** appears collapsed. Expand it to enable or disable individual families. Family preferences default to ON internally and survive turning the master off or restarting. Turning the master off hides Advanced options and uses baseline behavior for new searches.

GPU acceleration is a separate, optional switch and is **OFF by default**. It requires Smart Search, the relevant family preference, and a verified supported workload. Supported GPU paths include Wild, Static, Dream Radar, Hidden Grotto Pokemon, Pickup, and IV Cache Builder. Unsupported workloads and GPU failures retain the existing Smart CPU/baseline fallback behavior. Existing cache paths keep their normal priority.

No plugins are included. See the [v1.3.0 release notes and coverage matrix](docs/RELEASE_v1.3.0.md).

## Source

This repository contains the PokeFinder+ source code.

PokeFinder+ is based on [NickPlayeZ/PokeFinder](https://github.com/NickPlayeZ/PokeFinder), which is based on [Admiral-Fish/PokeFinder](https://github.com/Admiral-Fish/PokeFinder). See [LICENSE](LICENSE) and the [upstream README](docs/UPSTREAM_README.md).

## Build on Windows

Use MSVC x64, CMake 3.31 or newer, Ninja, Python 3.14, and Qt 6.10.1 x64.
Clone recursively to obtain the required submodules.

```powershell
git clone --recurse-submodules https://github.com/REddff2/PokeFinderPlus.git
cd PokeFinderPlus
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt 6.10.1 directory>"
cmake --build build --target PokeFinder PokeFinderPlusLauncher
```

For tests, configure with `-DTEST=ON`, build `PokeFinderTest`, and run `ctest --test-dir build --output-on-failure`.

For Gen 5 Smart Search verification, also configure with `-DSEARCH_OPTIMIZATION_TESTS=ON` and build all targets. The optional `SMART_SEARCH_IV_CACHE` and `SMART_SEARCH_SHA_CACHE` CMake paths enable the cache fixture tests.
