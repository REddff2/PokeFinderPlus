# PokeFinder+ v1.3.0

- Expanded Gen 5 smart search coverage.
- Expanded Gen 5 GPU acceleration coverage for verified workloads.
- Added per-family Gen 5 optimization controls.
- Added collapsible Advanced options in Settings, visible only when Smart Search is ON.
- Retained automatic Smart CPU/baseline fallback for unsupported workloads and GPU failures.

Gen 5 Smart Search and GPU acceleration are OFF by default. All nine family preferences default to ON internally. Disabling the master preserves family choices; Advanced options starts collapsed when enabled. Normal PokeFinder Threads behavior is unchanged. Changes apply to new searches and generation.

No plugins are included. The package has empty `plugins/` and `data/` directories and uses the surrounding normal PokeFinder release's Qt runtime.

## Coverage matrix

| Family | Verified Smart CPU coverage | Verified GPU coverage |
|---|---|---|
| Wild | Generator/searcher paths and supported Phenomenon Pokemon tabs using Wild | Guarded ordinary and independent-IV workloads |
| Static | Generator/searcher paths | Guarded independent-IV workloads |
| Event | Generator/searcher paths | Smart CPU fallback |
| Eggs | BW and B2W2 paths | Smart CPU fallback |
| Dream Radar | Generator/searcher paths | Guarded independent-IV workloads |
| Hidden Grotto | Pokemon and slot/item paths | Guarded Pokemon workloads; slots/items use Smart CPU |
| Pickup | Generator/searcher paths | Guarded independent-IV workloads |
| Adjacent Seeds | Bounded-MT calculation path | Smart CPU fallback |
| Cache Builders | IV-cache generation and SHA-cache membership lookup | Guarded IV-cache generation; SHA cache uses Smart CPU |

The master and owning family must be enabled for an added optimization. A disabled family uses baseline behavior even when GPU is selected. IDs, profile calibration, occurrence-only Phenomenon tools and other unlisted utilities remain native. Existing cache consumption keeps upstream priority. GPU workloads retain the existing size, IV-window, selectivity, device and safety guards; this release does not force GPU execution for unsupported searches.

## Package

Extract `PokeFinderPlus-v1.3.0.zip` into a normal PokeFinder release folder and run `PokeFinder+/PokeFinder+.exe`. The ZIP contains only the `PokeFinder+/` add-on: launcher, application, GPU helper, kernel, `qt.conf`, empty `plugins.ini`, and empty `plugins/` and `data/` directories. Qt DLLs, plugins, logs and test tools are not bundled.

## Verification

The final versioned release build passed:

- **636 upstream checks across 54 suites**, with no failures or skips.
- **11/11 registered regression groups**, including Gen 5 family equivalence and infrastructure/cache checks.
- **60 family-policy matrices and 30 GPU-dispatch matrices**, covering all nine family controls, disabled-family baseline behavior and cache ownership.
- Full-field, order and multiplicity comparisons for CPU and supported GPU paths on NVIDIA GeForce RTX 3080 and Intel UHD Graphics 770.
- Complete GPU searches for all five supported search families with only the owning family enabled, plus GPU IV-cache generation and exact cache-byte comparisons. Missing-device/helper, initialization, kernel, execution, timeout, invalid-output, cancellation and replay checks passed.
- Retained Wild legal-IV-domain CPU/GPU equivalence, exact raw GPU survivor indices, and stale-helper fallback checks.
- **60 existing IV/SHA cache-priority dispatch cases**.
- **19 separate-process family settings persistence cases**, plus **19 visibility/restart cases** covering clean OFF/OFF defaults, hidden Advanced options, expand/collapse without preference changes, and restoration of saved family choices.
- Real Gen 5 form checks and optional Annotations, IV Total and Slot Sprites compatibility checks in a separate test host. None of those plugins is included in this package.
- Clean packaged application and launcher shutdown, both with **exit code 0**, including checks at the final package path with an external Qt runtime.

The source audit matches 656 application source files to the verified TEST candidate; release changes only version identification to v1.3.0 and documents the release. Previous package files and upstream remain unchanged. The ZIP was checked for exact contents and CRC integrity; `plugins/` and `data/` are empty, and no Qt DLLs, logs, sources or test tools are bundled.

Only source and the v1.3.0 tag are pushed. GitHub Release publication is manual.
