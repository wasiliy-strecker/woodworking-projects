#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
: "${HOLZWERKPROJEKT_QT_ROOT:?Qt-6.8.3-SDK angeben}"
holzwerk_arch="$(uname -m)"
holzwerk_version="$(sed -n 's/project(HolzwerkProjekt VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
holzwerk_name="holzwerkprojekt-$holzwerk_version-macos-$holzwerk_arch"
holzwerk_stage="$PWD/build/releases/$holzwerk_name"
holzwerk_dmg="$PWD/build/releases/$holzwerk_name.dmg"
if [[ -e "$holzwerk_stage" || -e "$holzwerk_dmg" ]]; then
  echo "Lieferung existiert bereits: $holzwerk_name" >&2
  exit 1
fi
cmake --preset release -DBUILD_TESTING=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "-DCMAKE_OSX_ARCHITECTURES=$holzwerk_arch"
cmake --build --preset release --parallel 4
ctest --preset release
mkdir -p "$holzwerk_stage"
cp README.md THIRD_PARTY_NOTICES.md "$holzwerk_stage/"
cp -R docs "$holzwerk_stage/docs"
cp -R build/release/holzwerkprojekt.app "$holzwerk_stage/HolzwerkProjekt.app"
holzwerk_app="$holzwerk_stage/HolzwerkProjekt.app"
"$HOLZWERKPROJEKT_QT_ROOT/bin/macdeployqt" "$holzwerk_app" -always-overwrite -appstore-compliant
test -f "$holzwerk_app/Contents/PlugIns/sqldrivers/libqsqlite.dylib"
test -f "$holzwerk_app/Contents/PlugIns/platforms/libqcocoa.dylib"
test -f "$holzwerk_app/Contents/PlugIns/imageformats/libqjpeg.dylib"
mkdir -p "$holzwerk_app/Contents/Resources/translations"
cp "$HOLZWERKPROJEKT_QT_ROOT/translations/qtbase_de.qm" "$holzwerk_app/Contents/Resources/translations/"
cat > "$holzwerk_app/Contents/Resources/qt.conf" <<'QTCONF'
[Paths]
Plugins=PlugIns
Translations=Resources/translations
QTCONF
cp -R licenses "$holzwerk_app/Contents/Resources/licenses"
cp -R "$HOLZWERKPROJEKT_QT_ROOT/sbom" "$holzwerk_app/Contents/Resources/licenses/qt-sdk-sbom"
cp THIRD_PARTY_NOTICES.md "$holzwerk_app/Contents/Resources/"
holzwerk_icons="$PWD/build/HolzwerkProjekt.iconset"
mkdir -p "$holzwerk_icons"
for holzwerk_size in 16 32 128 256 512; do
  sips -z "$holzwerk_size" "$holzwerk_size" assets/branding/icon-1024.png --out "$holzwerk_icons/icon_${holzwerk_size}x${holzwerk_size}.png" >/dev/null
  holzwerk_double=$((holzwerk_size * 2))
  sips -z "$holzwerk_double" "$holzwerk_double" assets/branding/icon-1024.png --out "$holzwerk_icons/icon_${holzwerk_size}x${holzwerk_size}@2x.png" >/dev/null
done
iconutil -c icns "$holzwerk_icons" -o "$holzwerk_app/Contents/Resources/HolzwerkProjekt.icns"
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string HolzwerkProjekt.icns" "$holzwerk_app/Contents/Info.plist"
# Ad-hoc signing permits local Apple-Silicon testing. It is not a distribution identity.
codesign --force --deep --sign - "$holzwerk_app"
hdiutil create -volname HolzwerkProjekt -srcfolder "$holzwerk_stage" -ov -format UDZO "$holzwerk_dmg"
shasum -a 256 "$holzwerk_dmg" > "$holzwerk_dmg.sha256"
echo "$holzwerk_dmg"
echo "Native GUI-Abnahme und spätere Store-Signierung bleiben erforderlich."
