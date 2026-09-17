# Current PokeFinder+ packaging

The two-package setup is complete and was verified on 2026-09-06. This is the current packaging guide; STARTUP.md records the earlier Qt 6.11.2 startup investigation.

## Outputs

Clean add-on to distribute:
C:\PokeFinderDev\PokeFinderPlus-package\release\PokeFinder+\

Plugin development add-on:
C:\PokeFinderDev\PokeFinderPlus-package\test\PokeFinder+\

The clean add-on contains exactly:

```text
PokeFinder+/
  PokeFinder+.exe
  PokeFinderPlusApp.exe
  qt.conf
  plugins.ini       (empty/default)
  plugins/          (empty)
  data/             (empty)
```

There are zero DLLs anywhere inside the clean add-on, zero source/build/test files, and no diagnostic logs. The app does not create a diagnostic log with the clean default configuration. With no plugins installed, Plugins contains only Plugin Manager....

The test add-on contains those same core files, plugins/Annotations.dll, a plugins.ini enabling Annotations and diagnostic logging, and PokeFinderPlus.log produced by the application. The plugin's DLL and placeholder behavior are unchanged.

The release and test parent directories contain copies of the normal PokeFinder release/runtime for local launch verification. These parent files are outside the add-ons. Distribute only release/PokeFinder+, not its entire parent directory. Copy this add-on into a normal compatible PokeFinder release folder and run its PokeFinder+.exe.

## Runtime and preserved work

The distribution build uses Qt 6.10.1 to match the normal PokeFinder release tested here. The earlier Qt 6.11.2 executable failed against this parent with 0xC0000139, so a separate build was necessary. Both new add-ons use the parent folder's Qt DLLs and platforms/qwindows.dll; neither includes its own Qt runtime.

SDK: C:\PokeFinderDev\qt-sdk (official Qt 6.10.1 MSVC x64 archives)
Build: C:\PokeFinderDev\build-plus-parent

The original C:\PokeFinderDev\build-plus and working C:\PokeFinderDev\PokeFinderPlus-package\PokeFinder+ package remain preserved. The clean upstream checkout C:\PokeFinderDev\PokeFinder remains clean.

A clean copy was also installed and verified at:
C:\Users\Jack\Downloads\PokeFinder-Windows (1)\PokeFinder\PokeFinder+\

## Rebuild and regenerate both outputs

```powershell
& 'C:\PokeFinderDev\PokeFinderPlus-src\PokeFinderPlus\Package.ps1' `
  -ParentReleaseDirectory 'C:\Users\Jack\Downloads\PokeFinder-Windows (1)\PokeFinder'
```

The script locates MSVC, builds the core/launcher/Annotations with the separate SDK, and creates both outputs from explicit file lists. It checks that SDK and parent Qt versions match. For another Qt version, supply its matching -QtDirectory and a separate -BuildDirectory. Use -SkipBuild only to package existing compatible build outputs.

The clean output is created fresh with empty plugins and data directories and an empty plugins.ini. No existing plugin directory is copied into it. Only the test branch copies the explicitly named built Annotations.dll. Regeneration was exercised successfully. Previous generated add-ons are preserved under OutputDirectory/archive before replacement, including test plugins, settings, and data. The original working package outside release/test is never replaced by this script.

Logging is opt-in through plugins.ini:

```ini
[diagnostics]
logging=true
```

The test output enables it; the clean output leaves it off. No plugin API or annotation feature changes were made for packaging.

## Verification results

The final verification used launcher execution with SDK paths removed from PATH and Qt plugin environment variables cleared. Loaded-module paths confirmed use of each parent release's Qt runtime.

- Clean output: visible, usable main window; only Plugin Manager...; normal app and launcher exits 0; plugins/data remained empty; plugins.ini remained empty; no diagnostic log created.
- Test output: visible, usable main window; Annotations and Plugin Manager...; Annotations loaded and initialized; real mouse clicks disabled/re-enabled it, saved the state and kept the menu open; normal app and launcher exits 0; shutdown recorded in the diagnostic log. The test output was left enabled.
- Clean copy inside the original normal release folder: the same clean-menu, runtime-path, empty-folder, no-log, and exit-0 checks passed.
- The existing Annotations placeholder was verified separately using the build-only Qt verification host: the visible label read "Annotations plugin is working.", plugin shutdown returned, and the host exited 0. The flat core menu has no Open command, so the host exercises the existing exported window function without changing the core menu or plugin behavior. The host is not shipped in either add-on.

Final application SHA256 in both new outputs:
07E50CCC8CAB96816FF9E7B550161D4EA232164FCF7E34B65264B955D9679DC7

Evidence:
C:\PokeFinderDev\build-plus-parent\package-verification.txt
C:\PokeFinderDev\build-plus-parent\packaging-build.log
C:\PokeFinderDev\build-plus-parent\annotations-smoke.stdout.log

Repeat the package UI checks with:

```powershell
& 'C:\PokeFinderDev\PokeFinderPlus-src\PokeFinderPlus\verification\Verify-Packages.ps1' `
  -InstalledCleanDirectory 'C:\Users\Jack\Downloads\PokeFinder-Windows (1)\PokeFinder\PokeFinder+'
```

The verification script expects the generated test defaults (Annotations enabled), toggles off/on, and leaves it enabled. Qt 6.10 does not expose the menu TogglePattern used by the older harness, so the check uses actual mouse clicks, the persisted state, and menu visibility. Diagnostic helpers and screenshots stay in the build directory.
