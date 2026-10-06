# Packs the same files as a zip, for anyone who would rather not run an installer.
$root    = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$version = (Select-String -Path (Join-Path $root "installer\Somii.iss") -Pattern '#define AppVersion\s+"([^"]+)"').Matches[0].Groups[1].Value
$build   = Join-Path $root "build\Geminus_artefacts\Release"
$stage   = Join-Path $env:TEMP ("Somii-zip-" + [guid]::NewGuid().ToString("N"))
$dist    = Join-Path $root "dist"

foreach ($d in "VST3\Somii", "CLAP", "Standalone") { New-Item -ItemType Directory -Force (Join-Path $stage $d) | Out-Null }
New-Item -ItemType Directory -Force $dist | Out-Null

Copy-Item -Recurse (Join-Path $build "VST3\Somii.vst3") (Join-Path $stage "VST3\Somii")
Copy-Item (Join-Path $build "CLAP\Somii.clap") (Join-Path $stage "CLAP")
Copy-Item (Join-Path $build "Standalone\Somii.exe") (Join-Path $stage "Standalone")
Copy-Item (Join-Path $root "installer\README.txt") $stage
Copy-Item (Join-Path $root "installer\LICENSE.txt") $stage
@"
Manual install
--------------
1. Copy the folder VST3\"Somii" into
   C:\Program Files\Common Files\VST3\
2. Optional: copy CLAP\"Somii.clap" into C:\Program Files\Common Files\CLAP\
3. Rescan plug-ins in your DAW.
The standalone app needs no install: run Standalone\"Somii.exe".
"@ | Set-Content (Join-Path $stage "INSTALL.txt") -Encoding UTF8

$zip = Join-Path $dist "Somii-$version-Windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Remove-Item -Recurse -Force $stage
"zip: $zip"
