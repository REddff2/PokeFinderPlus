# PokeFinder+

Reconstructed pre-Search-Optimization v1, based on NickPlayeZ's PokeFinder 5.1.5.
This release was reconstructed from local source; it is not an exact historical
PokeFinder+ checkout.

PokeFinder+ adds branding, a Windows launcher, plugin discovery and loading,
headless plugin support, per-plugin data helpers, and proxy-safe result navigation.
Search behavior matches the upstream reference. This version has no PokeFinder+
Search Optimization or GPU search code.

See [the reconstruction record](RECONSTRUCTED-V1.md) for the upstream reference,
retained features, restored files, and verification results.

## Build on Windows

Use MSVC x64, CMake 3.31 or newer, Ninja, Python 3.14, and Qt 6.10.1 x64.
Clone recursively to obtain the pinned zstd and encounter-table submodules.

    git clone --recurse-submodules https://github.com/REddff2/PokeFinderPlus.git
    cd PokeFinderPlus
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt 6.10.1 directory>"
    cmake --build build --target PokeFinder PokeFinderPlusLauncher

For the upstream regression suite, configure with -DTEST=ON, build PokeFinderTest,
and run ctest --test-dir build --output-on-failure.

## Runtime

Place PokeFinder+.exe, PokeFinderPlusApp.exe, qt.conf, an empty plugins.ini, and
empty plugins/ and data/ directories inside a PokeFinder+/ directory within a
matching normal PokeFinder release. The launcher uses its parent's Qt runtime.
The v1 add-on ships no plugins or Qt DLLs. Optional plugin implementations and
their source verification tools are present in this repository.

## Attribution

PokeFinder+ builds on [NickPlayeZ/PokeFinder](https://github.com/NickPlayeZ/PokeFinder)
and [Admiral-Fish/PokeFinder](https://github.com/Admiral-Fish/PokeFinder).
See [the original upstream README](docs/UPSTREAM_README.md) and [LICENSE](LICENSE).
