# Plugin data locations

All files created by a PokeFinder+ plugin belong under:

```text
PokeFinder+/data/<stable-plugin-id>/
```

Use the inline helpers in `PluginApi.hpp`:

```cpp
PokeFinderPlus::pluginDataRoot();
PokeFinderPlus::pluginDataDirectory(QStringLiteral("Annotations"));
```

The root is `QDir(QCoreApplication::applicationDirPath()).filePath("data")`.
In the host process, this is beside `PokeFinderPlusApp.exe`, inside the running
PokeFinder+ add-on. It is independent of the current working directory, the
plugin DLL location, and the parent Nick PokeFinder runtime folder.

Use a stable ID, not a translated display name or DLL filename. IDs contain
only ASCII letters, digits, underscores, or hyphens. Choose a portable directory
name (avoid Windows reserved device names such as `CON`). Keep the ID and its
casing unchanged between versions. Annotations reserves `Annotations`.
Invalid IDs or an unavailable application directory return an empty string;
callers must handle that result rather than passing it to file operations.

Path lookup never creates a directory. A missing directory is normal. Create it
with `QDir::mkpath()` only when there is actual data to write. For example, inside
a plugin's save operation (with `QSaveFile` included):

```cpp
const QString directory =
    PokeFinderPlus::pluginDataDirectory(QStringLiteral("Annotations"));
if (directory.isEmpty() || !QDir().mkpath(directory))
    return false; // Report the write failure through the plugin's normal UI.

QSaveFile file(QDir(directory).filePath(QStringLiteral("settings.json")));
if (!file.open(QIODevice::WriteOnly))
    return false;
if (file.write(serializedSettings) != serializedSettings.size())
    return false;
return file.commit();
```

Keep settings, saved annotations, caches, temporary files, and plugin logs in
that plugin's directory or its subdirectories. Do not use the parent PokeFinder
folder, the location beside `PokeFinder.exe`, the working directory, or `plugins/`.
Use controlled relative filenames beneath the resolved directory; never fall
back to another location when writing fails. This is a plugin authoring
convention, not filesystem sandboxing of third-party DLLs.

These helpers add no exports or initialization arguments and keep API version
1 unchanged. Existing plugins continue to work without rebuilding. New plugins
can use the helpers without requiring a new host executable.

Annotations currently writes no persistent data. No persistence is introduced
by this convention. Its future files must use `data/Annotations/`.

Packaging continues to provide empty `plugins/` and `data/` directories in the
clean add-on. Do not include test plugins or data in the clean release. Probe
plugins and test writes belong only in the TEST package and must be cleaned up
after verification.
