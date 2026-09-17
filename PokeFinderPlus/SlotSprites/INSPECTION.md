# Slot Sprites — pre-implementation inventory

Inspected 2026-09-09, active source: `C:/PokeFinderDev/PokeFinderPlus-src`.
Deployment target: `C:/PokeFinderDev/PokeFinderPlus-package/test/PokeFinder+` only.
All paths below are relative to the active source. No upstream/core/other-plugin changes are needed for identity mapping.

## Result tables

`n: species (form)` means the original translated name and form text, including the encounter slot number. Invalid/suppressed `-` cells must remain undecorated. “Full” means species, numeric form, result gender and result shiny state are available; it does not mean every form has distinct artwork.

| Model class | Window/view | Current Slot or equivalent text | Source field | Pokémon identity | Item identity | Safe capability |
|---|---|---|---|---|---|---|
| WildGeneratorModel3 | Gen 3 Wild, generator | `n: species (form)` | `TableModel<WildGeneratorState>::getItem(row)` | Full, gated by `isValid()` | State has `getItem()`, but no item display | Pokémon at Slot |
| WildSearcherModel3 | Gen 3 Wild, searcher | `n: species (form)` | `WildSearcherState` | Full | No item display | Pokémon at Slot |
| PokeSpotModel | Gen 3 Tools / Poké Spot | `n: species` | `PokeSpotState` | Species, gender, shiny; no form field | None | Pokémon at Slot, default form |
| WildGeneratorModel4 | Gen 4 Wild, generator | `n: species (form)` or `-` | `WildGeneratorState4` | Full; invalid or species-zero rows must be skipped | `getItem()`, separate Item column | Pokémon at Slot, outlined item at Item |
| WildSearcherModel4 | Gen 4 Wild, searcher | `n: species (form)` | `WildSearcherState4` | Full | `getItem()`, separate Item column | Both in their existing columns |
| PokeRadarModel4 | Gen 4 Poké Radar, generator AND searcher | `n: species (form)` or `-` | `PokeRadarState::getPokemon()` / `getSearcherPokemon()` | Full when `hasPokemon()`; select searcher state only in searcher mode with `hasSearcherPokemon()` | Matching Pokémon state's `getItem()` | Both; never interpret patch/Shiny-patch columns as Pokémon results |
| WildGeneratorModel5 | Gen 5 Wild AND Phenomenon, generator | `n: species (form)`, `S: species`, or `-` | `WildState5` | Full on valid Pokémon results; `getPhenomenonItem()` suppresses Pokémon identity | `getItem()`, separate Item column, including item-only phenomenon results | Both, with explicit row-kind guards |
| WildSearcherModel5 | Gen 5 Wild AND Phenomenon, searcher | As above | `SearcherState5<WildState5>::getState()` | Full on Pokémon results | Same as generator | Both, with explicit row-kind guards |
| HiddenGrottoSlotGeneratorModel5 | Gen 5 Hidden Grotto, grotto generator | `n (species gender)` or `n: item`, invalid `-` | `HiddenGrottoState` | `getData()` is species ONLY when `!getItem()`; gender present; no form/shiny | `getData()` is item ONLY when `getItem()` | Pokémon OR outlined item in the same Slot cell; check validity |
| HiddenGrottoSlotSearcherModel5 | Gen 5 Hidden Grotto, grotto searcher | As above | `SearcherState5<HiddenGrottoState>::getState()` | As above | As above | As above; columns shift with Pass Power |
| HiddenGrottoGeneratorModel5 | Hidden Grotto, Pokémon generator | No Slot/species column | `State5` | Row lacks species/form; selection elsewhere is needed | None | Skip; do not infer from current UI selection after a search |
| HiddenGrottoSearcherModel5 | Hidden Grotto, Pokémon searcher | No Slot/species column | `SearcherState5<State5>` | Same limitation | None | Skip |
| PhenomenonGeneratorModel5 | Legacy standalone model; no current Form constructor found | Item name OR literal `Pokemon` in Slot | `PhenomenonState` | **Unavailable**: non-item state does not identify a species; data is uninitialized in that constructor | `getData()` valid ONLY if `getItem()` | Items only; leave generic Pokemon text untouched |
| PhenomenonSearcherModel5 | Legacy standalone model; no current Form constructor found | As above | `SearcherState5<PhenomenonState>::getState()` | Unavailable | As above | Items only |
| PickupGeneratorModel5 | Gen 5 Pickup, generator | `n: species (form)` or `-`; Item 1–6 and Held Item separate | `PickupState::getWild()` optional `WildState5` | Full only with embedded wild result | Six `getItem(slot)` with `getActive(slot)` guards; optional wild held item | Pokémon at Slot, items in the seven existing item columns |
| PickupSearcherModel5 | Gen 5 Pickup, searcher | No Pokémon Slot; Item 1–6 | `SearcherState5<PickupState>::getState()` | No Pokémon display | Six item IDs and active flags | Items only |
| WildModel8 | Gen 8 BDSP Wild | `n: species (form)` | `WildState8` | Full | `getItem()`, separate Item column | Both |
| UndergroundModel | Gen 8 BDSP Underground | `species` in Species column | `UndergroundState` | Species, gender, shiny; no form field | `getItem()`, separate Item column | Both; default Pokémon form |

The inventory comes from all `Model` headers and implementations plus model construction in `Form`. No current Slot cell was found that safely identifies a species from a bare slot number alone. The generic legacy Phenomenon `Pokemon` value is deliberately unresolved. Other Egg, Static, Event, Dream Radar, GameCube, ID, Profile, IV/PID and Researcher tables lack a Slot/species/item result column suitable for this plugin; do not add sprites from guesses about their current controls. Advance Finder views may share/proxy supported models and should map each index through the full proxy chain.

## Common data access and sorting

`Model/TableModel.hpp` is a template, not a common runtime species API. It exposes a const `getItem(row)`. Typed adapters can read numeric result states after identifying a known concrete model and mapping every proxy index to its source. Compile against this exact source/toolchain; do not parse localized display names into species IDs or modify model roles. This approach needs no plugin API or host exports and preserves sorting and all RNG data. Model schemas vary with mode; locate columns using the known model's translated headers, not one global column number.

## Mapping and offline packaging

Source: https://github.com/msikma/pokesprite . Inspect and pin its commit, `data/pokemon.json`, `data/item-map.json`, and actual asset paths before packaging.

Pokémon: numeric national species ID indexes `pokemon.json`; use its English slug and `gen-8.forms`, including `is_alias_of` and `has_female`. Host numeric forms are documented in `Core/Resources/i18n/en/forms_en.txt`; create an explicit numeric-form map to metadata form keys (e.g. Unown `!` -> `exclamation`, Basculin Blue -> `blue-striped`). Never derive the file path from displayed names. Unknown form/gender uses a documented default fallback. Resolve exact form/gender/shiny first, then form, then default normal species; unresolved/missing files render original text alone. Shiny must come from the result's `getShiny()`.

Items: host `Translator::getItem(u16)` indexes `items_en.txt` directly. Validate numeric IDs against PokéSprite `item_XXXX` entries (e.g. Oran Berry) and use the mapped relative path beneath `items`. Zero/unknown item IDs produce no sprite. Do not use names as filenames. Both Pokémon and held items should occupy their existing separate columns where available.

Use `PokeFinderPlus::pluginDataDirectory("SlotSprites")/sprites`. Package only Gen 1–5 species/form variants reachable by the inspected Gen 3/4/5/BDSP models and relevant mapped normal items where practical. Include PokéSprite's MIT license and attribution clarifying that artwork is copyright Nintendo / Creatures / GAME FREAK. The later self-install update adds WinHTTP only when local assets are incomplete; use bounded lazy pixmap/scaled-pixmap caches, cleared on shutdown.

## First milestone and compatibility finding

First table: **WildGeneratorModel3**, Gen 3 Wild generator, Slot column. Its state unambiguously exposes species, form, gender, shiny, and validity. Implement and verify Pokémon sprites plus unchanged text before enabling more adapters or item sprites.

`Annotations/AnnotationController.cpp::bindingFor()` requires `typeid(*view->itemDelegate()) == typeid(QStyledItemDelegate)`. `apply()` also refuses per-column/per-row custom delegates. Therefore installing a Slot Sprites delegate prevents later annotations; wrapping an existing AnnotationDelegate also disrupts Annotations' binding identity/lifecycle. This cannot be solved by ordinary delegate wrapping while leaving Annotations frozen.

Proposed compatible exception: a **per-table Qt style hook beneath the existing delegate**, adding a local decoration to the `QStyleOptionViewItem` passed to native Qt rendering. Leave delegate and model pointers untouched. AnnotationDelegate already sends its tint/selection palette through this path; IV Total proxies can be mapped to their source states. No overlays, Win32 drawing, table replacement, or global styles. Skip unknown custom delegates/styles rather than claim compatibility. The custom-delegate requirement needs clarification before implementing this alternative.

Start with 34x28 Pokémon / 24x24 item bounding boxes, nearest-neighbor scaling, and no row-height mutation (shrink to fit short rows). Save and restore only plugin-changed column widths, and handle model resets, model swaps, late windows, destruction, disable/re-enable, and shutdown. Verification must include actual Annotations and IV Total implementations, both activation orders, and native selections, not just mock delegates.

## Resolution after inspection

The user explicitly approved the per-table style hook, with no Annotations/IV Total changes. The first real Gen 3 table milestone passed before coverage expanded. The final implementation leaves row heights and column widths under host/user control and supplies a decorated resize-to-contents hint; it does not save or mutate widths, avoiding stale IV Total header snapshots. The original style pointer/inheritance state is restored on disable. See README.md and VERIFICATION.md for the final behavior and recorded tests.
