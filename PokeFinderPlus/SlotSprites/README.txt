Slot Sprites — TEST plugin

INSTALL
Copy only SlotSprites.dll into PokeFinder+\plugins\.
Launch PokeFinder+.exe and enable Plugins > Slot Sprites.
For a quick check, open Gen 3 > Wild and generate results: sprites appear
beside the existing Slot text. Annotations and IV Total can run alongside it.
Pokemon choices in that window also show sprites before their names, both
in the closed selector and in its dropdown. Tools > IV Calculator provides
an editable selector with sprite-decorated completion results.

FIRST ACTIVATION
The DLL validates local assets first. If they are missing or incomplete, a
small cancellable Qt progress window downloads a pinned PokéSprite ZIP over
HTTPS using native Windows WinHTTP on a worker thread. It closes on success.
The DLL contains its ZIP reader and install manifest. No Git, Python,
PowerShell, OpenSSL, Qt TLS backend, or additional runtime DLL is required.
The plugin still requires a compatible PokeFinder+/Qt host ABI.

DATA LOCATION
PokeFinder+\data\SlotSprites\
  sprites\pokemon-gen8\...     Pokemon, forms, gender and shiny variants
  sprites\items\...            Normal item sprites (not items-outline)
  data\pokemon.json
  data\item-map.json
  manifest.json                Numeric result-to-sprite mapping
  installed.json               Pinned revision, layout version, UTC timestamp
  POKESPRITE_ATTRIBUTION.txt
  POKESPRITE-LICENSE.md
  MINIZ-LICENSE.txt

Temporary download/extraction uses download-temp\ inside this same folder.
The plugin resolves this path with pluginDataDirectory("SlotSprites").
It never writes to the parent PokeFinder runtime folder.

VALIDATION AND OFFLINE USE
The pinned revision is c5aaa610ff2acdf7fd8e2dccd181bca8be9fcb3e from
https://github.com/msikma/pokesprite
The archive URL is https://codeload.github.com/msikma/pokesprite/zip/
followed by that revision. There is no automatic upstream update check.
The DLL extracts only its allowlisted 2,423 PNGs, two mapping JSON files,
and upstream license. Every required file must match its embedded size and
SHA-256 hash; both mapping files must parse. The installation marker alone
never proves the assets are complete.

Validated staging folders are renamed into place; old folders are retained
for rollback until final validation succeeds. The completion marker is
written last. Rendering attaches only after the complete install succeeds.
An interrupted/incomplete installation is rejected and retried next time.
If an OS/filesystem error prevents rollback, previous files remain under
download-temp\previous with RECOVERY_REQUIRED.txt instead of being deleted.

With valid local assets, activation makes ZERO network calls and shows no
setup window. It works offline afterward. Missing assets with no connection
produce one native Qt error; the plugin remains inactive. Enable it again
to retry. Cancel leaves it inactive, removes temporary data, and shows no
failure message. A pending WinHTTP operation may take up to its timeout to
finish cancellation; the GUI event loop remains responsive.

NETWORK DETAILS
Normal Windows HTTPS certificate validation; no verification bypasses.
System proxy discovery; up to five redirects, with HTTPS-to-HTTP blocked.
HTTP 200 required; empty, partial and oversized downloads rejected.
10-second network phase timeouts, a 180-second deadline checked between
operations, and a 32 MiB archive limit. Progress crosses threads through
atomic counters polled by the Qt GUI thread. Unknown-length downloads show
an indeterminate progress bar.

RENDERING
The existing per-table Qt style hook is retained. Models, delegates, result
text, sorting, selection/focus, annotation colors and table dimensions stay
under the host's control. Disable/shutdown restores the original style.
Only the 16 supported result models are decorated; unknown styles/delegates
are skipped. Normal items come from items/, Pokemon from pokemon-gen8/.
There are 175 mapped items and species 1–898 with applicable variants.
Unknown identities keep the original text. PNG cache behavior is unchanged.

POKEMON SELECTORS AND LISTS
Verified Wild3/4/5/8, PokeSpot, Phenomenon, Pickup, Hidden Grotto slot-filter,
EggSettings, IV Calculator and Chained SID combos use their numeric species
data. Encounter forms are decoded from the host's packed species/form value.
Event4/5/8, Encounter Lookup and GameCube Seed Finder name-only choices use
exact, unambiguous names from all eight host translations. No fuzzy matching.
Generic species choices use normal, non-shiny sprites. Unknown forms and
unexpected data types are skipped rather than inferred from their labels.

Native DecorationRole icons serve the closed combo, popup and autocomplete
list. The original models, delegates, text, selections and completion behavior
are retained. Existing icons are respected. PNGs load lazily through the same
bounded cache as table sprites. New controls use the existing 34x28 Pokemon
bounds; table sizes and item sprites are unchanged.

Underground's Pokemon checklist decorates only its QListView popup, using a
scoped native styled delegate with the existing checkbox event handling.
Its Any/multiple-selection summary stays plain. Disable restores original
decoration roles, delegates and icon sizes while keeping selections/checks.
New windows and model changes are discovered through events/model signals.

APP-WIDE SELECTORS
Static3/4/5/8, GameCube, all 12 Dream Radar selectors, Hidden Grotto Pokemon
selectors, both PokeRadar Settings grids and Raids now have validated adapters.
IV Calculator alternate forms, parent Ditto entries and the Unown discovered
letter checklist are also covered. Native closed displays, dropdown rows and
editable autocomplete share the same icons. This is no longer Wild-only.

Some PokeRadar chain-slot names omit East/West form while both are available;
those ambiguous rows stay plain. Grouped Unown puzzle ranges are not individual
Pokemon forms. Unrelated labels/lists/trees remain unchanged. See the source
FINAL-COVERAGE.md for the complete inventory and automated TEST evidence.

ATTRIBUTION
PokéSprite by msikma: https://github.com/msikma/pokesprite
Artwork copyright Nintendo / Creatures / GAME FREAK. Relevant upstream MIT
license and copyright notices are stored beside the installed assets.
ZIP reader: miniz 3.0.2, revision 293d4db1b7d0ffee9756d035b9ac6f7431ef8492,
MIT license, compiled into SlotSprites.dll. https://github.com/richgel999/miniz

TEST BUILD ONLY
Developed for the matching PokeFinderPlus-src checkout and Qt 6.10.1/MSVC.
TEST launcher: C:\PokeFinderDev\PokeFinderPlus-package\test\PokeFinder+\PokeFinder+.exe
No host/core, launcher, PluginManager, Annotations or IV Total changes.
No release/final package deployment. See source VERIFICATION.md for results.

