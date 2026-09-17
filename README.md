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
   ├─ qt.conf
   ├─ plugins.ini
   ├─ plugins\
   └─ data\
```

## Plugins

Plugins are optional DLL files placed in `PokeFinder+\plugins\`.

No plugins are included with the base PokeFinder+ download.

## Source

This repository contains the PokeFinder+ source code.

PokeFinder+ is based on [NickPlayeZ/PokeFinder](https://github.com/NickPlayeZ/PokeFinder), which is based on [Admiral-Fish/PokeFinder](https://github.com/Admiral-Fish/PokeFinder). See [LICENSE](LICENSE) and the [upstream README](docs/UPSTREAM_README.md).

v1 is a [reconstructed pre-Search-Optimization build](RECONSTRUCTED-V1.md).

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
