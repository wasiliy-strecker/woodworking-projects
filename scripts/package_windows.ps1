param(
  [ValidateSet("zip", "msix")][string]$Format = "zip",
  [string]$QtRoot = $env:HOLZWERKPROJEKT_QT_ROOT,
  [string]$IdentityName,
  [string]$Publisher,
  [string]$PublisherDisplayName
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Set-Location (Split-Path -Parent $PSScriptRoot)
if (-not $QtRoot) { throw "HOLZWERKPROJEKT_QT_ROOT muss auf Qt 6.8.3 MSVC 2022 x64 zeigen." }
if ($Format -eq "msix" -and (-not $IdentityName -or -not $Publisher -or -not $PublisherDisplayName)) {
  throw "MSIX benötigt IdentityName, Publisher und PublisherDisplayName aus Partner Center. Keine Platzhalter ausliefern."
}
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
  & $Program @Arguments
  if ($LASTEXITCODE -ne 0) { throw "$Program ist fehlgeschlagen: $LASTEXITCODE" }
}
$version = [regex]::Match((Get-Content CMakeLists.txt -Raw), 'project\(HolzwerkProjekt VERSION ([\d.]+)').Groups[1].Value
$name = "holzwerkprojekt-$version-windows-x64"
$output = Join-Path $PWD "build/releases"
$stage = Join-Path $output "$name-$Format"
$artifact = Join-Path $output "$name.$Format"
if ((Test-Path $stage) -or (Test-Path $artifact)) { throw "Lieferung existiert bereits: $artifact" }
$env:CMAKE_PREFIX_PATH = $QtRoot
$env:PATH = "$QtRoot/bin;$env:PATH"
Invoke-Checked "cmake" @("--preset", "release", "-DBUILD_TESTING=ON")
Invoke-Checked "cmake" @("--build", "--preset", "release", "--parallel", "4")
Invoke-Checked "ctest" @("--preset", "release")
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item "build/release/holzwerkprojekt.exe" $stage
Invoke-Checked (Join-Path $QtRoot "bin/windeployqt.exe") @("--release", "--no-compiler-runtime", "--translations", "de", "--dir", $stage, (Join-Path $stage "holzwerkprojekt.exe"))
$crt = Join-Path $env:VCToolsRedistDir "x64/Microsoft.VC143.CRT"
if (-not (Test-Path $crt)) { throw "MSVC-Redistributables fehlen. In einer VS-2022-x64-Developer-Shell ausführen." }
Copy-Item (Join-Path $crt "*.dll") $stage
$drivers = Join-Path $stage "sqldrivers"
New-Item -ItemType Directory -Path $drivers -Force | Out-Null
Get-ChildItem $drivers -File | Remove-Item
Copy-Item (Join-Path $QtRoot "plugins/sqldrivers/qsqlite.dll") $drivers
New-Item -ItemType Directory -Path (Join-Path $stage "translations") -Force | Out-Null
Copy-Item (Join-Path $QtRoot "translations/qtbase_de.qm") (Join-Path $stage "translations")
foreach ($required in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "platforms/qwindows.dll", "imageformats/qjpeg.dll", "sqldrivers/qsqlite.dll", "msvcp140.dll", "vcruntime140.dll")) {
  if (-not (Test-Path (Join-Path $stage $required))) { throw "Laufzeit fehlt: $required" }
}
Copy-Item "licenses" (Join-Path $stage "licenses") -Recurse
Copy-Item "docs" (Join-Path $stage "docs") -Recurse
Copy-Item (Join-Path $QtRoot "sbom") (Join-Path $stage "licenses/qt-sdk-sbom") -Recurse
Copy-Item "README.md", "THIRD_PARTY_NOTICES.md" $stage
@"
[Paths]
Prefix=.
Plugins=.
Translations=translations
"@ | Set-Content (Join-Path $stage "qt.conf") -Encoding ascii
if ($Format -eq "zip") {
  Compress-Archive -Path $stage -DestinationPath $artifact
} else {
  $manifest = Get-Content "packaging/windows/AppxManifest.xml.in" -Raw
  $manifest = $manifest.Replace("@IDENTITY_NAME@", [Security.SecurityElement]::Escape($IdentityName)).Replace("@PUBLISHER@", [Security.SecurityElement]::Escape($Publisher)).Replace("@PUBLISHER_DISPLAY_NAME@", [Security.SecurityElement]::Escape($PublisherDisplayName))
  $manifest = $manifest.Replace('Version="0.1.0.0"', "Version=`"$version.0`"")
  $manifest | Set-Content (Join-Path $stage "AppxManifest.xml") -Encoding utf8
  New-Item -ItemType Directory -Path (Join-Path $stage "Assets") -Force | Out-Null
  Copy-Item "assets/branding/Square44x44Logo.png", "assets/branding/Square150x150Logo.png", "assets/branding/StoreLogo.png" (Join-Path $stage "Assets")
  Invoke-Checked "makeappx.exe" @("pack", "/d", $stage, "/p", $artifact)
}
((Get-FileHash $artifact -Algorithm SHA256).Hash.ToLower() + "  " + (Split-Path $artifact -Leaf)) | Set-Content "$artifact.sha256" -Encoding ascii
Write-Output $artifact
Write-Output "Native GUI-Abnahme mit isoliertem --data-dir erforderlich. Keine Signierung und kein Upload ausgeführt."
