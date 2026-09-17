# Final app-wide milestone

See [FINAL-COVERAGE.md](FINAL-COVERAGE.md) for current coverage and verification. Earlier milestone evidence below is retained as history.

# Self-install update — 2026-09-09

Final update replaces item lookups with normal `items/` artwork and adds a
WinHTTP worker-thread installer. Rendering, adapters and caches are retained.

Actual frozen TEST launcher checks, using the production SlotSprites.dll:

| Check | Result |
| --- | --- |
| A: empty asset folder | One real WinHTTP HTTPS session, one setup window, validated installation, actual sprite rendering, exit 0 |
| B/C: restart with assets and WinHTTP forced offline | Zero WinHTTP sessions, zero setup windows, normal rendering and all three plugins, exit 0 |
| D: missing assets and WinHTTP forced offline | One native error, Slot Sprites checkmark off, temporary files removed, exit 0 |
| Cancellation | Setup cancels, checkmark off, no failure dialog or temporary files, exit 0 |
| E: compatibility | Actual Annotations Mark button, IV Total proxy, sprites/tint/text/sorting and restoration pass |

Offline testing uses a temporary TEST-only probe to intercept the production
DLL's WinHttpOpen import and return ERROR_WINHTTP_CANNOT_CONNECT. The same
probe counts sessions and checks whether calls run on the GUI thread. It
changes no firewall/proxy/network settings and adds no production test switch.
This is simulated network unavailability, not a physical cable disconnect.
All network calls were off the GUI thread; GUI timers continued during setup.
Real downloads took roughly 4–15 seconds in these runs. Temporary probes were
removed and plugins.ini restored byte-for-byte after each run.

The final source build also passed all 16 real model fixtures, 250,001 real
Emerald results, normal-item/shiny-Pokemon rendering, mixed Grotto rendering,
and all 36 activation/shutdown permutations. A missing Oran Berry PNG and a
same-length corrupted PNG were each rejected despite an existing marker.
The item artwork was visually inspected in the rendered Underground model
and matched the pinned normal `items/berry/oran.png`, not items-outline.
All 175 mapped items have pinned hashes; 1,619 PNGs are required in total.

Dependency inspection shows public Qt Widgets/Gui/Core, WINHTTP, existing
MSVC CRT, and normal Windows system libraries. No QtNetwork, Qt private ZIP
symbols, TLS backend, OpenSSL, miniz DLL or scripting/tool dependency.

Evidence: verification/self-install/{online,reuse,missing,cancel}/,
verification/rendering-v2.log, verification/setup-online.log and
verification/final-audit.json. Final distributable: dist/SlotSprites.dll and
dist/README.txt. The TEST asset folder is left installed for normal use.

Limits: offscreen native Qt widgets were used to avoid desktop interference.
Normal Windows certificate checks and redirect policy are enabled in code;
certificate failures, every HTTP status and filesystem rollback failures
were not individually fault-injected. No arbitrary future Qt/host ABI
compatibility is claimed. Protected host/other-plugin hashes are unchanged.

---
# Slot Sprites verification

2026-09-09. TEST only. First milestone was restricted to WildGeneratorModel3; its real generated results, rendering, normal/shiny mapping, coexistence and lifecycle passed before adding the remaining adapters and item support.

## Automated verification host

- All 16 supported real host model classes tested with typed result fixtures, including dynamic Gen 4/5 columns, Poké Radar generator/searcher choice, Hidden Grotto mixed kinds, unavailable identities, inactive/zero Pickup items and BDSP's separate Species/Item display.
- 250,001 results produced by the real Emerald WildGenerator3. First shiny found at advance 9025. Species checked against 1,000 generated rows; sorted rows checked through proxy mapping.
- Normal and shiny artwork, female and numeric-form mappings, Oran Berry numeric ID, unknown species/forms and missing files exercised.
- Actual SlotSprites.dll loaded/unloaded. Model/delegate identity, unchanged text, unchanged geometry, native selected/focused background and pixel-identical original rendering after disable checked.
- Existing Annotations and IV Total controllers compiled unchanged for compatibility checks; all 36 activation/shutdown order combinations pass restoration checks.
- New windows, model reset/new searches, window destruction, disable/re-enable and shutdown tested.
- Item and Pokémon rendering in separate real Underground model columns and mixed Hidden Grotto rows verified; unknown custom delegates intentionally skipped.

## Unchanged TEST application

The temporary guarded probe ran via `PokeFinder+.exe`, with the existing deployed Annotations.dll and IVTotal.dll, and the new SlotSprites.dll. The launcher and application binaries were not rebuilt or modified.

- Real Gen 3 Wild window generated 100,001 results; shiny Zubat at advance 9025 resolved to shiny artwork and was visually inspected.
- Actual Plugins menu checkmarks enabled/disabled the plugins.
- All three DLLs active: actual Annotations Mark button accepted Slot; annotation background retained with sprites, including after disabling/re-enabling Slot Sprites.
- Sorting remained attached to real source results through IV Total's actual proxy chain.
- 100 scroll/repaint steps over the large result table completed in 195 ms on the initial full host probe. This is an offscreen measurement, not a guarantee of interactive frame rate on other machines.
- Alternate shutdown order restored original model/delegate/style and preserved column widths.
- Gen 4 table opened after activation gained the hook. A subsequent actual Gen 3 search updated sprites correctly.
- Application and launcher exited **0** through normal Qt shutdown with Slot Sprites active. Temporary probe DLL removed and prior plugins.ini restored.

The process ran with Qt's offscreen platform to avoid interfering with the desktop; actual widgets, models, generator, delegates and frozen DLLs were used. Screenshots were rendered by those widgets and visually inspected. Every possible encounter/form/item and every interactive Windows theme was not manually exercised. Unknown custom styles/delegates are deliberately unsupported. The generic legacy Phenomenon Pokemon result remains unresolved by design.

Evidence is in `verification/` in the source checkout, and `data/SlotSprites/verification/` in TEST. Protected-file SHA-256 comparisons cover the frozen host source components, both other plugins' source, and existing TEST binaries. No clean upstream, release, or final files were written.

