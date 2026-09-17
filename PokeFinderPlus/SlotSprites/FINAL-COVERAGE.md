# Slot Sprites: final app-wide coverage

The plugin now covers Pokémon selectors across generations 3, 4, 5 and 8, including utilities. It is no longer effectively Wild-only. This inventory applies to the frozen `C:/PokeFinderDev/PokeFinderPlus-src` checkout and actual TEST executable.

Inspection included all `Translator::getSpecie/getSpecies` and `getSpecieNames` uses in Form/Model, every `.ui` item containing an exact Pokémon name, custom ComboBox/ComboBoxProxy/CheckList implementations, dynamic PokéRadar construction, completion models, and standalone list/tree controls.

| Form/window | Pokémon-listing controls | Identity | Status |
| --- | --- | --- | --- |
| Static3/4/5 | Generator and Searcher Pokémon selectors, all categories | Original template index + category | **New** |
| Static8 | Pokémon selector, all categories | Template index + category | **New** |
| GameCube | Generator/Searcher static and shadow-team Pokémon selectors | Template index + category, separate shadow data | **New** |
| DreamRadar | Six Generator and six Searcher species selectors | Original template index; None stays plain | **New** |
| HiddenGrotto | Pokémon-tab Generator/Searcher selectors | Group/slot ID + source location row through sort proxy + game version | **New** |
| PokeRadar | Both dynamic Settings grids: Pokémon, replacements, named Chain Slot choices | Packed species/form or species IDs; slot names cross-check sibling species/form choices | **New**, ambiguous-form exception below |
| Raids | Species selector in every region, normal/rare dens and 69 events | Den/rarity/event row + game version; actual form/gender/forced shiny/G-Max data | **New** |
| IVCalculator | Alternate-form selector in every game | Selected numeric species + actual form row + game | **New**; species extended through 898 |
| EggSettings inside Eggs3/4/5/8 | Parent A/B Ditto entries | Exact localized name; gender symbols stay plain | **New** |
| ProfileEditor4 | Discovered Unown A–Z checklist popup | Species 201 + exact letter/form | **New** |
| Wild3/4/5 | Generator/Searcher Pokémon filters | Packed species/form | Existing, retained |
| Wild8, PokeSpot | Pokémon filter | Packed species/form | Existing, retained |
| Wild4, Wild8 | Replacement Pokémon selectors | Numeric species | Existing, retained |
| HiddenGrotto | Grotto-tab Generator/Searcher Pokémon filters | Numeric species | Existing, retained |
| Phenomenon, Pickup | Pokémon filters | Packed encounter species/form | Existing, retained |
| IVCalculator, ChainedSID | Species selector | Numeric species | Existing, retained |
| EggSettings inside Eggs3/4/5/8 | Egg species selector | Numeric species | Existing, retained |
| EncounterLookup, Event4/5/8 | Pokémon/species selectors | Exact localized name; no species ID stored | Existing, retained |
| GameCubeSeedFinder | Gales own/enemy lead and Colosseum party lead | Exact localized name | Existing, retained |
| Underground | Pokémon checklist QListView popup | Species in UserRole+1 | Existing, retained |
| Wild3/4/5/8, PokeSpot, PokeRadar, Underground, HiddenGrotto slot results, Pickup/Phenomenon results | Existing named Pokémon/item cells, 16 model classes | Actual typed row state through proxies | Existing rendering unchanged; see INSPECTION.md |
| RoamerMap | Named Entei/Raikou/Latias/Latios display | Native host already supplies Pokémon images | Already illustrated by host |

## Views and lifecycle

Native DecorationRole icons serve QComboBox-derived controls, closed displays, QListView popup rows, and QCompleter's native completion proxies/popups. Editable controls retain typing/completion. Their private standard item models and delegates stay in place; existing icons are preserved. Persistent indices track owned decorations through sorting and row changes.

Underground and Unown CheckList popups use a scoped styled delegate preserving check state and forwarding checkbox interaction to the original delegate. Their Any/multiple-selection summaries stay plain. The table style hook, sprite sizes, item rendering and bounded cache implementation are unchanged. There is no global style replacement or screen-coordinate overlay.

PokéRadar's unnamed controls are recognized through the owning form and validated Settings grid/control types. Template/slot indices are never mistaken for species IDs. `verification/ExportControlData.cpp` exports encounter metadata from the matching frozen core. Display species and Raid star text are cross-checked before using that metadata. Profile game names are recognized exactly in all eight host languages. The host's default Raid profile uses a non-Sword version; its Den implementation explicitly chooses Shield data in that case, which the adapter follows.

Disable/shutdown restores owned decoration roles, original icon sizes and checklist delegates, preserving current selection/check state. New windows and model changes are discovered through events/signals. Unknown models, identities, custom delegates or styles fail safely.

## Remaining undecorated entries

* Some PokéRadar Chain Slot rows omit the form while the Pokémon selector contains both East and West forms. The UI contract cannot establish which form belongs to the slot. Those rows remain text-only; the explicit form choices have sprites. Exhaustive profile/location testing recorded 116 such row occurrences, not 116 separate missing controls.
* ProfileEditor4 Unown puzzle ranges A–J/R–V/K–Q/W–Z represent groups, not one form. The individual discovered-letter checklist is decorated.
* None, dash, Any, generic Pokemon, bare slot numbers, invalid identities and unknown forms stay plain.
* Static/Egg/Event/DreamRadar/GameCube/Raid and HiddenGrotto Pokémon-result tables display statistics without a Pokémon-name column. No columns or inferred result identity are inserted; their named selectors are covered.
* JirachiAdvancer, GameCubeSeedFinder input history and ProfileCalibrator5 standalone QListWidgets contain instructions/search inputs/needles. No additional Pokémon-listing QTreeView/QTreeWidget or standalone list view/widget was found. Unrelated controls and plain static labels remain unchanged.

No other named Pokémon-listing control was found uncovered in this source/runtime inspection. This is coverage of the inspected host, not arbitrary future models.

## Forms and assets

Species 1–898, 2,423 required PNGs and the same 175 normal item sprites are mapped. All previous species/form mappings and item mappings compare identical. New mappings cover every form row exposed by the IV Calculator's games and Raid regional/gender/shiny/G-Max variants. Arceus's Gen4 type order is handled separately. Visually identical forms share upstream artwork. Alcremie uses the exposed cream form with plain accessory artwork because the host does not expose the sweet.

Ordering was checked against host personal/encounter data and [PKHeX form definitions](https://github.com/kwsch/PKHeX/blob/master/PKHeX.Core/PKM/Util/Conversion/FormConverter.cs). Shared Appletun/Flapple G-Max artwork is documented in [PokéSprite issue 110](https://github.com/msikma/pokesprite/issues/110). Milcery/Hattrem event entries carrying a G-Max factor use their own species artwork; they have no corresponding transformation sprite.

WinHTTP/miniz installer code is unchanged. Old installations upgrade once through the existing atomic install path; complete installations make zero network calls. Assets remain under TEST `PokeFinder+/data/SlotSprites`. Only SlotSprites.dll is manually installed; README.txt is optional.

## Verification

Temporary probes execute inside the real frozen TEST application. The broad suite checks 56,026 Pokémon rows across 64 ordinary Pokémon controls: all Static categories/profiles, all 12 Dream Radar selectors, all Grotto locations/both games, both PokéRadar grids, all Raid regions/dens/rarities/events, and every IV Calculator game/species/form combination. All 26 Unown checklist forms are checked separately. Logs enumerate every opened form and retain a runtime inventory.

Focused checks cover real keyboard selection, editable typing/autocomplete, closed/popup sprite pixels, checklist toggling, model repopulation/sorting/replacement, prior icons, exact Japanese names, invalid identities and widget destruction. Selected runtime screenshots were visually inspected.

With all three deployed plugins active, disable restores all 552 inspected combos' model/delegate/icon size/decoration role/text. Re-enable restores 6,726 decorated rows. Actual Annotations marking/tint, IV Total proxy sorting, result text/dimensions and existing rendering pass. The separate table suite passes all 16 model classes and all 36 activation/shutdown-order combinations, including DLL unload.

TEST exits normally through the launcher with code 0. Online setup runs WinHTTP off the GUI thread. Reuse mode blocks network entry and records zero calls. Probe DLLs are removed and plugins.ini restored byte-for-byte. Fixture profiles live under TEST data; the profile-path setting is temporarily redirected for the test process and restored afterward without writing the user's profile file. The test helper is not shipped.

These are automated real-window tests using Qt's offscreen platform, not manual mouse testing of every row. Evidence is under `verification/self-install/online` and `reuse`; table results are in `verification/final-table-regression.log`. Protected-file and previous-mapping audits accompany this report.
