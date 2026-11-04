# Qt sources and replacement on macOS

Woodworking Projects uses the dynamic Qt 6.8.3 Core, Gui, Widgets and Sql libraries
and Qt platform/SQLite plugins under LGPL version 3. Additional Qt base
dependencies are identified in the bundled SPDX inventory. Qt translations are
embedded without modification. See `THIRD_PARTY_NOTICES.md` and `licenses/`.

You may modify and replace these libraries and investigate the application to
debug those modifications. The publisher does not restrict these rights. No
publisher private signing key, account or online activation is needed for the
local replacement procedure below. This does not grant a general public license
to the publisher's own application source code.

## Sources supplied with this application

In Finder, choose **Show Package Contents** for the app. `Contents/Resources/QtSources`
contains the complete unmodified Qt base and Qt translations 6.8.3 source archives,
their original download URLs and SHA-256 checksums. The files are supplied with
every Store installer; obtaining them does not depend on an external download
service. Original library license texts are in `Contents/Resources/licenses`.
The SDK's SPDX inventories in `licenses/qt-sdk-sbom` identify its components.

## Building a compatible Qt base

Use a macOS development machine with Xcode command-line tools, CMake and Ninja.
Extract `qtbase-everywhere-src-6.8.3.tar.xz` into a directory you control. Build
dynamically linked frameworks with the same Qt minor version and the architectures
you intend to run. For the universal app, retain both arm64 and x86_64 and macOS 12:

```sh
cmake -S qtbase-everywhere-src-6.8.3 -B qtbase-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_INSTALL_PREFIX="$PWD/qt-local"
cmake --build qtbase-build --parallel 4
cmake --install qtbase-build
```

Preserve the public Qt 6.8 ABI, framework names and required Core/Gui/Widgets/Sql,
Cocoa and QSQLITE functionality. Changes that intentionally break the ABI require
matching changes to the dependent application and are not compatible replacements.
Qt's full build documentation is included in its sources and available at
<https://doc.qt.io/qt-6.8/macos-building.html>.

## Installing a local replacement

Create a new folder with copies of the modified `Qt*.framework` bundles. Retain
their `@rpath` install names and framework dependencies as used by the original
bundled frameworks. `otool -L` displays these names; `install_name_tool -id` and
`-change` can update build-directory paths to their bundled `@rpath` counterparts.
Use a compatible set when changing several Qt modules. Python 3 and the Xcode
command-line `codesign` tool are needed by the supplied helper:

```sh
python3 '/Applications/Woodworking Projects.app/Contents/Resources/replace_macos_qt.py' \
  --app '/Applications/Woodworking Projects.app' \
  --replacement '/absolute/path/to/replacement-frameworks' \
  --output "$HOME/Applications/Woodworking Projects Local Qt.app"
```

The helper creates a new app, replaces only frameworks already used by it,
removes the publisher's provisioning profile from that copy and signs the copy
locally. It assigns a separate `.local-relinked` bundle ID and keeps the app
sandbox enabled. Your original app and its collection remain untouched. Launch
the copy from Finder. Export JSON from the original and import it into the copy
if you want to use the same collection. No uninstall or global security-setting
change is part of this process. A locally changed app does not retain Apple's
distribution signature, Store updates or the publisher's support guarantee.

Compatible replacement plugins may similarly be installed in the copied
`Contents/PlugIns` directory and locally signed before re-signing the enclosing
app with the sandbox entitlements. Never put new libraries into the original
Store installation in place, and preserve a backup of your collection.

The release report records the actual replacement/startup check performed for
this version. This procedure establishes a technical replacement route; it is
not a general legal determination for every Store term or every future Qt build.
