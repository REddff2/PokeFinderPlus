# PokeFinder+ branding

The supplied artwork is preserved byte-for-byte in resources/PokeFinderPlus-256.png.
resources/pokefinder-plus.ico contains 16, 24, 32, 48, 64, 128 and 256 px RGBA
images. The original upstream ICO has 16, 24, 32, 48 and 256 px entries.
GenerateIcon.ps1 regenerates the ICO using high-quality bicubic downscaling,
retaining the original 256 px PNG frame and alpha transparency. It does not
redesign, crop, stretch or recolor the artwork.

Branding.cmake embeds the ICO into both executable targets through branding.rc.in
and gives the Qt application a separate :/PokeFinderPlus/pokefinder-plus.ico resource.
The original appicon.rc, Form/Images/pokefinder.ico and Form/resources.qrc remain intact.

Branding.cpp sets QApplication's window icon and applies it when top-level windows
are shown, covering inherited forms that explicitly select the original icon.
It does not change window flags, modality, parenting or plugin lifecycle behavior.
main.cpp calls this helper. No .ui files were edited.

Form/MainWindow.cpp changes only the product-name portion of its existing title:
QString("PokeFinder+ %1").arg(POKEFINDER_VERSION).
The existing Form/version.h.in -> generated/Form/version.h mechanism remains
unchanged. No version literal was added to branding code. The verified current
title is PokeFinder+ 5.1.5. QApplication's settings name and organization remain
unchanged; an explicit applicationDisplayName override is deliberately avoided
because Qt on Windows appends it to native window titles.

## Verification, 2026-09-08

- Both executable PE resources contain all seven exact ICO frames.
- Windows Explorer's shell icon API returns the new artwork for both EXEs.
- The real app's native small/large icons use the supplied artwork for its title,
  taskbar and Alt+Tab representation; the visible title is exactly PokeFinder+ 5.1.5.
- MainWindow and Static3 use the new Qt icon, including Static3's explicit old UI icon.
- TEST: Annotations cell/row marks, Clear, Clear All, close/reopen and menu state pass.
  IV Total activates, calculates 186 for six perfect IVs, sorts numerically and disables.
- FINAL: existing Annotations DLL remains installed and disabled by default; marking,
  clearing, close/reopen and menu-state regression checks pass.
- Actual TEST and FINAL app/launcher processes all exit normally with code 0.
- Supplied PNG, upstream icon/resources, version template, plugin code, launcher code,
  qt.conf, packaging scripts, clean release and normal Nick executables remain unchanged.

FINAL preserves its Annotations-only plugin set. IV Total remains in TEST.
Final folder: C:/PokeFinderDev/PokeFinderPlus-package/final/PokeFinder+
Final ZIP: C:/PokeFinderDev/PokeFinderPlus-package/PokeFinderPlus-Final.zip
The distributable has only the two EXEs, qt.conf, plugins.ini, plugins/Annotations.dll,
and an empty data directory. It contains no test files, logs or bundled Qt DLLs.
