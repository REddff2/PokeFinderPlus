# Slot Sprites (TEST)

Self-installing Pokémon and normal item sprites for the frozen PokeFinder+ checkout at `C:/PokeFinderDev/PokeFinderPlus-src`.

Enable **Plugins > Slot Sprites**. Existing and subsequently opened supported result tables gain icons beside their original text. Disable the checkmark to restore their original style. A temporary setup window appears only when assets need installing. See [README.txt](README.txt) for installation, validation and offline behavior.

## Coverage

App-wide coverage now includes Static, GameCube, Dream Radar, Hidden Grotto, PokéRadar, Raids and utility selectors. It decorates verified species selectors,
their closed display, popup rows and autocomplete lists. Underground's
Pokémon checklist gets popup sprites while retaining its plain selection
summary. See [CONTROLS.md](CONTROLS.md) for the inspected controls, numeric
and localized-name contracts, and the remaining ambiguous entries.

These new controls use native DecorationRole icons with a lazy QIconEngine;
their original models and combo delegates stay in place. Persistent indices
restore owned icons after sorting/model changes; pre-existing icons are left
alone. The checklist alone uses a scoped styled popup delegate. Both features
reuse the table sprite cache and the existing 34×28 Pokémon bounds. Disable
removes the decorations and restores original icon sizes/delegate pointers.
The expanded species/form assets install once through the existing atomic upgrade path.

Gen 3 Wild generator/searcher and Poké Spot; Gen 4 Wild generator/searcher and Poké Radar; Gen 5 Wild/Phenomenon generator/searcher, Hidden Grotto slot generator/searcher, Pickup generator/searcher; Gen 8 BDSP Wild and Underground. Legacy Phenomenon models support only identified item results. There are 16 supported model classes (Poké Radar uses one class in two modes). See [INSPECTION.md](INSPECTION.md) for the complete inventory and row-level restrictions.

Where a table has both Pokémon and Item columns, each receives its corresponding icon. A Hidden Grotto Slot cell receives either Pokémon or item artwork according to its result kind. Generic `Pokemon`, invalid `-`, zero species, zero items, and unavailable identities remain undecorated.

## Rendering and compatibility

The user approved a per-table `QProxyStyle` hook because Annotations requires the exact original delegate type. Slot Sprites changes only a copy of `QStyleOptionViewItem` in the existing delegate's item-paint path. It never replaces the delegate or model, changes roles, writes result states, inserts proxy models, or changes RNG/sorting values. Annotation palettes and native focus/selection pass through unchanged.

The hook clones a matching Qt factory style without taking ownership of the original. It is installed only on known supported table models and restored on disable/shutdown, including whether the table originally inherited its style. Unknown custom styles, stylesheet styles and unknown custom delegates are intentionally skipped. Per-row/column custom delegates are left alone and receive no icons. Qt's global application style is never replaced.

Rows and column widths are **not changed programmatically**. This avoids stale saved header widths when IV Total is disabled in another order. Normal host/user “resize to contents” uses the decorated size hint. Narrow columns retain native elision; widen them normally if needed. Pokémon fit within 34×28 logical pixels and items within 24×24, shrinking to existing short rows. Transparent padding is trimmed in memory before nearest-neighbor scaling; packaged PNGs are unchanged. Device pixel ratio is respected.

Existing Annotations behavior when IV Total replaces a model is unchanged; Slot Sprites does not migrate or persist annotation marks across another plugin's model replacement. Marking cells/rows while all three are active is supported.

## Mapping, assets and memory

Result adapters use const access to the actual typed result states after mapping every proxy index to its root. This DLL is built for the matching frozen host/model ABI, not arbitrary PokeFinder versions. Reinspect and rebuild before using with changed host model/state layouts.

`prepare_assets.py` validates the host's numeric form definitions and resolves metadata form aliases and female flags. Paths come from PokéSprite metadata and are checked against the pinned repository tree. Item IDs map directly through `item-map.json`; all 175 nonzero IDs in the inspected host item dictionary have normal assets. Unknown forms fall back to a normal default species sprite; missing variants/files fail safely. Shiny comes from the actual result state, never patch flags or unrelated fields.

PokéSprite source: https://github.com/msikma/pokesprite

Pinned commit: `c5aaa610ff2acdf7fd8e2dccd181bca8be9fcb3e`.

Only SlotSprites.dll is distributed with the optional README.txt. On first activation, WinHTTP downloads the pinned GitHub archive on a worker thread. Compiled-in miniz extracts the 2,423 required PNGs, two mapping files, and license. SHA-256 validation, staged rename/rollback and a completion marker protect installation. Complete local assets are reused with no network activity. See [README.txt](README.txt) for the exact layout and failure behavior.

Normal items use items/; Pokémon use pokemon-gen8/. Original and scaled pixmaps still load lazily into bounded 8 MiB caches each.

## Build and TEST deployment

This directory is an independent CMake project. It does not edit the frozen host CMake configuration, packaging architecture, launcher, PluginManager, API, qt.conf, Annotations, or IV Total.

From PowerShell in this directory:

```powershell
./Build.ps1
./Deploy-Test.ps1
```

The build uses the matching MSVC toolchain and `C:/PokeFinderDev/qt-sdk`. `SLOT_SPRITES_VERIFY` defaults on to build verification targets; production `SlotSprites.dll` links Qt Widgets/Gui/Core, the existing C++ runtime, and normal Windows system libraries including WinHTTP. The verification executable links the frozen core library and compiles the actual supported model implementations and existing plugin controllers without modifying them.

Run build/SlotSpritesSetupVerify.exe online once (with matching Qt DLL paths)
for its isolated development assets, then run SlotSpritesVerify.exe.
Deploy-Test.ps1 copies only the DLL. No package-data directory is deployed.
Use -VerifyHost -NetworkTest online -FreshAssets for an explicit clean TEST
installation; -NetworkTest reuse forces WinHTTP offline and verifies no calls.
The missing and cancel modes verify inactive failure/cancellation through the
unchanged host. Temporary probes are removed and plugins.ini restored.
Verification executables and probe DLLs must never be shipped as plugins.

prepare_assets.py regenerates resources/bootstrap.json from the pinned vendor
archive/metadata and exact host. It is a developer tool, not an installation
requirement. No PNGs, archives, TLS backends, or external runtime binaries are
embedded in SlotSprites.dll. The DLL uses public Qt APIs only.

See VERIFICATION.md for evidence and limits; README.txt describes distribution.

