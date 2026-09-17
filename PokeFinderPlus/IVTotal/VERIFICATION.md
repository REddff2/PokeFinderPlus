# IV Total verification — 2026-09-07

Built with MSVC and the existing Qt 6.10.1 build at `C:/PokeFinderDev/build-plus-parent`.
Deployed only to `C:/PokeFinderDev/PokeFinderPlus-package/test/PokeFinder+`:
`PokeFinderPlusApp.exe` and `plugins/IVTotal.dll`.
Annotations, launcher, plugin API, packaging, upstream, release and final package were not changed.

## Implementation

`IVTotalPlugin.cpp` exports API version/name/initialize/shutdown only. It has no window export.
`IVTotalController.hpp/.cpp` discovers existing/new QTableViews via Qt widget events and checks
model replacement on paint using cached model pointers. There is no polling timer.
`IVTotalProxyModel.hpp/.cpp` inserts a row-preserving calculated column after Spe, returning an
integer sum. An outer QSortFilterProxyModel handles numeric row sorting. Existing source models
and existing sort/filter proxies remain intact underneath. Original roles, flags, and row-formatting
roles are forwarded. Source layout changes remap persistent indexes; structural row changes are
forwarded. Reset and column-schema changes invalidate affected indexes appropriately.

Each binding keeps guarded view/model/selection pointers, restores the original model and
selection model on shutdown, and bridges selection changes to preserve existing host listeners.
Model recognition uses an explicit inventory of 35 class names plus translated header inspection.
The original table's Show Stats checkbox is located per inspected Filter ownership, including
shared Advance Finder and Poke Radar views. Unknown display-mode ownership is excluded safely.
No result objects or actual IV values are modified. No settings/data files are created by IV Total.

## Approved host changes

PluginManager.cpp, setPluginEnabled():

- Only plugins exporting open_window have newly created windows tracked for hide/close events.
- Initialize the final enabled check with `plugin.initialized && !plugin.openWindow`, followed
  by the existing visible-window check for windowed plugins.
- Existing state persistence, shutdown, discovery, menu and Annotations lifecycle remain in place.

New generic helper: `Model/ModelIndexMapping.hpp` supplies `toSource`, `sourceRow` and `fromSource`.
It handles any number of QAbstractProxyModel layers. Row actions use column zero as the row
identity, so selecting a calculated column still targets the original result correctly.
The helper and all host actions contain no IV Total-specific condition or dependency.

| Host source file | Functions changed | Regression result |
|---|---|---|
| Form/Gen3/Static3.cpp | seedToTime | Same seed/window inputs before and after total sorting |
| Form/Gen3/Wild3.cpp | seedToTime | Same seed/window inputs before and after total sorting |
| Form/Gen4/Eggs4.cpp | calcPoketch; seedToTime | Same Poketch instructions and seed/window inputs |
| Form/Gen4/Event4.cpp | seedToTime | Same seed/window inputs |
| Form/Gen4/Static4.cpp | seedToTime | Same seed/window inputs |
| Form/Gen4/Wild4.cpp | seedToTime | Same seed/window inputs |
| Form/Gen4/PokeRadar.cpp | jumpToBattleAdv; markSelectedPatches; markSearcherPatches; seedToTime | Same battle result, rendered patch marks and seed/window inputs |
| Form/Gen5/HiddenGrotto.cpp | openAdjacentSeeds | Same destination window inputs |
| Form/Gen5/Phenomenon.cpp | openAdjacentSeeds | Same destination window inputs |
| Form/Gen5/Static5.cpp | openAdjacentSeeds | Same destination window inputs |
| Form/Gen5/Wild5.cpp | openAdjacentSeeds | Same destination window inputs |
| Form/Gen5/Tools/AdjacentSeeds.cpp | generate; updatePreview | Live generation selects actual target; preview remains correct and connected |
| Form/Util/AdvanceFinder.cpp | jumpToAdvance | Same original result in all 11 IV-bearing caller views |

The CMake change is confined to adding the IVTotal shared-library target in PokeFinderPlus/CMakeLists.txt.

## Verification performed

- Qt QAbstractItemModelTester checks on the projection and sorting proxy.
- Known totals 2, 10, 100 and 186 sort numerically in both directions.
- Original columns continue sorting; remaining columns retain their order after the inserted column.
- Persistent original-cell and calculated-cell indexes survive outer sorting and inner source layouts.
- Data changes, row insertion/removal, model reset, column-schema changes and source destruction.
- All 35 model classes / 41 main tables instantiated in actual PokeFinder forms and populated with
  typed deterministic result fixtures. Arithmetic, sorting, show-stats exclusion, new batches,
  disable/restore, re-enable, resize/scroll and window destruction passed for each.
- All 11 IV-bearing Advance Finder caller views tested with their real filtering and jump controls.
- Real RNG generation in Static3 with two different seeds, and real Adjacent Seeds calculation,
  checked alongside the fixture tests. The other forms used deterministic result fixtures rather
  than exhaustive game/method/search combinations.
- Actual egg inheritance display leaves a blank total instead of treating A/B as zero.
- Egg Gen4 game-dependent IV column changes tested between Diamond and HeartGold.
- Replacement of an unrelated model by a recognized IV model in an existing visible table detected.
- Headless activation creates no window. Both enabled and disabled states restored across separate processes.
- Annotations opens, X closes/unchecks, reopens, and menu disable closes it. Closing Annotations
  leaves IV Total active. No Annotations source or DLL was rebuilt or replaced.
- Real TEST launcher and compiled app started with both plugins active and exited normally: app 0, launcher 0.

## Model/table coverage

All rows below passed the actual-form fixture tests; none of these 35 classes is intentionally unsupported.
All use translated source headers HP / Atk / Def / SpA / SpD / Spe. Physical indexes are discovered
from the current model rather than fixed header-array offsets.

| Model class | Source window(s) and table(s) | Host sorting before activation |
|---|---|---|
| `EggModel3` | Form\Gen3\Eggs3.cpp : tableViewEmerald, tableViewRSFRLG | Direct TableModel; sorting not enabled |
| `GameCubeGeneratorModel` | Form\Gen3\GameCube.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `GameCubeSearcherModel` | Form\Gen3\GameCube.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `PokeSpotModel` | Form\Gen3\Tools\PokeSpot.cpp : tableView | Direct TableModel; sorting not enabled |
| `StaticGeneratorModel3` | Form\Gen3\Static3.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `StaticSearcherModel3` | Form\Gen3\Static3.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `WildGeneratorModel3` | Form\Gen3\Wild3.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `WildSearcherModel3` | Form\Gen3\Wild3.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `EggGeneratorModel4` | Form\Gen4\Eggs4.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `EggSearcherModel4` | Form\Gen4\Eggs4.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `EventGeneratorModel4` | Form\Gen4\Event4.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `EventSearcherModel4` | Form\Gen4\Event4.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `StaticGeneratorModel4` | Form\Gen4\Static4.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `StaticSearcherModel4` | Form\Gen4\Static4.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `WildGeneratorModel4` | Form\Gen4\Wild4.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `WildSearcherModel4` | Form\Gen4\Wild4.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `AdjacentSeedsModel` | Form\Gen5\Tools\AdjacentSeeds.cpp : tableView | Existing SortFilterProxyModel; sorting enabled |
| `DreamRadarGeneratorModel5` | Form\Gen5\DreamRadar.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `DreamRadarSearcherModel5` | Form\Gen5\DreamRadar.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `EggGeneratorModel5` | Form\Gen5\Eggs5.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `EggSearcherModel5` | Form\Gen5\Eggs5.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `EventGeneratorModel5` | Form\Gen5\Event5.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `EventSearcherModel5` | Form\Gen5\Event5.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `HiddenGrottoGeneratorModel5` | Form\Gen5\HiddenGrotto.cpp : tableViewPokemonGenerator | Direct TableModel; sorting not enabled |
| `HiddenGrottoSearcherModel5` | Form\Gen5\HiddenGrotto.cpp : tableViewPokemonSearcher | Existing SortFilterProxyModel; sorting enabled |
| `PickupGeneratorModel5` | Form\Gen5\Pickup.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `StaticGeneratorModel5` | Form\Gen5\Static5.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `StaticSearcherModel5` | Form\Gen5\Static5.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `WildGeneratorModel5` | Form\Gen5\Wild5.cpp : tableViewGenerator; Form\Gen5\Phenomenon.cpp : tableViewGenerator | Direct TableModel; sorting not enabled |
| `WildSearcherModel5` | Form\Gen5\Phenomenon.cpp : tableViewSearcher; Form\Gen5\Wild5.cpp : tableViewSearcher | Existing SortFilterProxyModel; sorting enabled |
| `EggModel8` | Form\Gen8\Eggs8.cpp : tableView | Direct TableModel; sorting not enabled |
| `StaticModel8` | Form\Gen8\Raids.cpp : tableView; Form\Gen8\Event8.cpp : tableView; Form\Gen8\Static8.cpp : tableView | Direct TableModel; sorting not enabled |
| `UndergroundModel` | Form\Gen8\Underground.cpp : tableView | Direct TableModel; sorting not enabled |
| `WildModel8` | Form\Gen8\Wild8.cpp : tableView | Direct TableModel; sorting not enabled |
| `PokeRadarModel4` | Form/Gen4/PokeRadar.cpp: generator.tableView and searcher.tableView | Generator direct; searcher uses SortFilterProxyModel with sorting enabled |

There are 35 distinct source-model classes and 41 main-table uses in this inventory, before shared Advance Finder views.

## Additional shared views

Form/Util/AdvanceFinder.cpp wraps its supplied result model in IndexFilterProxyModel and displays it in tableView. IV-bearing callers: Gen4 Eggs4, Event4, Static4, Wild4, PokeRadar; Gen5 Eggs5, Event5, Static5, Wild5, Phenomenon, and the HiddenGrotto Pokemon generator. The HiddenGrotto slot-generator caller has no IV block. Advance Finder does not enable header sorting.

## Required exclusions and display modes

- Show Stats replaces IV values with calculated stats without changing these six headers. A numeric 0-31 range check alone cannot distinguish low stats from IVs. The common TableModel does not supply an IV-specific role or a generic showStats property.
- Egg models in Gen3/4/5/8 can replace inherited IVs with A/B letters. Do not interpret missing/nonnumeric values as zero.
- PokeRadar has rows without a Pokemon (displaying dashes); no fabricated numeric total is appropriate.
- PickupGeneratorModel5 contains the IV block; PickupSearcherModel5 does not. Phenomenon uses WildGeneratorModel5/WildSearcherModel5 for its Pokemon results; PhenomenonGeneratorModel5/PhenomenonSearcherModel5 themselves have no IV block.
- HiddenGrottoGeneratorModel5/HiddenGrottoSearcherModel5 are Pokemon models. HiddenGrottoSlotGeneratorModel5/HiddenGrottoSlotSearcherModel5 have no IV block.
- PIDToIVModel displays all six IVs joined into one IVs column (headers Seed / Method / IVs). ChainedSID uses QStandardItemModel with one joined IVs column (IVs / Ability / Gender / Nature). Neither is a six-column table under the requested insertion rule.
- IV Calculator uses labels with possible IV ranges, not six exact-value result columns. IVToPIDModel, ResearcherModel, IDs, profiles, seed/time, Search Calls, Search Coin Flips, and Encounter Lookup have no six-IV result block.


## Test sources

Tests/CMakeLists.txt builds standalone verification executables against an existing compatible
PokeFinder+ build. Set POKEFINDER_BUILD_DIR and CMAKE_PREFIX_PATH when configuring it.
`proxy_tests` uses synthetic Qt models; `integration` links the real PokeFinder forms/models
and loads the actual plugin DLL through PluginManager; `host_regression` covers the lifecycle
and has enable/disable/disabled arguments for separate-process persistence checks.

Run integration and host_regression only from the TEST package so their application directory
resolves to its plugins folder. Back up/restore plugins.ini around test runs. Integration isolates
host test profiles and QSettings under TEST/data/IVTotalVerification; remove that test-created
directory and the temporary verification executables afterward. They are not plugin persistence.
No verification executable or test data belongs in release/final packages.
