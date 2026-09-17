Current distribution and test package instructions are in [PACKAGING.md](PACKAGING.md). This file preserves the earlier Qt 6.11.2 startup investigation.

# PokeFinder+ startup verification — 2026-09-06

The source fork opens successfully with Qt 6.11.2. The upstream checkout at C:\PokeFinderDev\PokeFinder was not modified; git status --short remains empty. All builds used the existing C:\PokeFinderDev\build-plus configuration.

## Concrete startup causes

1. Directly launching the packaged PokeFinderPlusApp.exe with only Windows directories on PATH exits before main() with unsigned exit code 3221225781 (0xC0000135). Its Qt DLLs are in the parent package directory, which is not automatically searched by Windows for this nested EXE. dumpbin /dependents lists Qt6Core, Qt6Gui, Qt6Widgets and Qt6Network as direct dependencies.
2. Adding the parent runtime directory to PATH resolves DLL loading but, without Qt plugin path configuration, Qt reports: Could not find the Qt platform plugin "windows". It searches PokeFinder+/platforms, while the matching qwindows.dll is in ../platforms. The initial surviving process in this scenario was a startup error dialog, not a working main window.
3. Running with both the exact SDK bin and SDK plugin paths succeeds. The original fork and untouched vanilla build both displayed their normal MainWindow using Qt 6.11.2. The fork loaded and initialized Annotations through its own QLibrary code. No source change or disabling plugin initialization was needed to reach the main window.

No evidence implicated OpenSSL, embedded resources, or the MainWindow/plugin initialization additions in the startup failure.

## Changes made in this phase

- Added Run-Development.ps1 beside this file. It starts the real source-built EXE directly with child-process Qt 6.11.2 paths, checks the Qt version, waits for normal close, and reports the real exit code. It does not change the machine PATH.
- Added qt.conf beside this file and copied it to C:\PokeFinderDev\PokeFinderPlus-package\PokeFinder+\qt.conf. Prefix=.. and Plugins=. make Qt search the parent package folder for platforms and styles. Keep this configuration beside the packaged PokeFinderPlusApp.exe. It does not change the Annotations directory, which remains applicationDirPath()/plugins.
- Fixed a separately reproduced Enabled/Open state bug in PluginManager.cpp. Previously, enabling a plugin that was disabled at startup saved true but never initialized it or enabled Open. Enabling now initializes; disabling calls shutdown; Open tracks successful initialization. Opening is guarded by that same state.
- Rebuilt the existing targets and copied the app, launcher and Annotations DLL to the isolated PokeFinder+ package. No annotation/highlighter features were added.

The Qt path configuration follows https://doc.qt.io/qt-6/qt-conf.html and https://doc.qt.io/qt-6/deployment-plugins.html.

## Run directly in the development environment

From PowerShell:

```powershell
& 'C:\PokeFinderDev\PokeFinderPlus-src\PokeFinderPlus\Run-Development.ps1'
```

Add -DebugPlugins to print Qt plugin diagnostics. The script defaults to C:\Qt\6.11.2\msvc2022_64 and C:\PokeFinderDev\build-plus. The runner itself was executed and verified: visible enabled MainWindow, normal close, exit 0.

The corrected package can be opened using:

```text
C:\PokeFinderDev\PokeFinderPlus-package\PokeFinder+\PokeFinder+.exe
```

Double-clicking the nested PokeFinderPlusApp.exe alone still requires its parent runtime directory on PATH. qt.conf fixes Qt's plugin lookup after Windows loads the DLLs; it does not configure Windows DLL lookup. Use the development runner for direct execution or the package launcher for ordinary use.

## Runtime verification

Windows UI Automation inspected the actual visible Qt windows and invoked their native menu actions; these are runtime checks, not compilation-only checks.

- MainWindow was visible, enabled, and remained open. Plugins appeared immediately after Tools.
- Plugins > Annotations contained Enabled and Open.
- Open displayed the native dialog with the exact label: Annotations plugin is working.
- Disabled the plugin through its menu: Open became disabled and plugins.ini contained enabled=false.
- Closed and restarted: Enabled remained off and Open remained disabled.
- Enabled it through its menu: initialization logged success, Open became enabled, plugins.ini contained enabled=true, and the placeholder opened.
- Tools > Researcher generated 10 rows using LCRNG with seed 0. The first result was 00006073. This is a focused check of existing functionality, not an exhaustive regression test.
- The app stayed responsive after using the plugin and Researcher; normal close returned 0x00000000. PokeFinderPlus.log recorded shutdown Annotations.
- After qt.conf was added, the package was tested directly with its parent on PATH and no Qt plugin environment variables, from C:\Users\Jack. It was also tested through the launcher with only Windows directories on PATH. Each main window remained visible and enabled for at least 10 additional seconds after startup. Both app exits and the launcher exit were 0x00000000.
- Loaded-module inspection confirmed package-local Qt6 DLLs, ../platforms/qwindows.dll, and PokeFinder+/plugins/Annotations.dll; no SDK plugin path was needed for these package runs.

Matching final app SHA256, in build-plus and the package:
B15D1CD0D96E7CEBAF8E3CDFB95264BECA9BD193E5B3E2DE0FC5A0C52F972139

Evidence is in C:\PokeFinderDev\build-plus:
- PokeFinderPlus.log (plugin load, initialize, disable, shutdown)
- no-runtime-path.stderr.log (empty: Windows loader failure before application logging)
- parent-runtime-path.stderr.log (pre-fix Qt platform search failure)
- package-verification.txt (final direct/launcher UI state, loaded paths and exits)
- package-fixed-direct.stderr.log and package-fixed-launcher.stderr.log (final Qt diagnostics)
- development-runner.stdout.log and development-runner.stderr.log
- Verify-Plus.ps1 and Verify-Package.ps1 (diagnostic harnesses; Verify-Plus toggles the stored enabled state)

Portability to a clean machine, Qt 6.10.1 compatibility, and comprehensive network/TLS packaging remain outside this phase.
