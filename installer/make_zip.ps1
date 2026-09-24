# Packs the same files as a zip, for anyone who would rather not run an installer.
$root    = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$version = (Select-String -Path (Join-Path $root "installer\002.iss") -Pattern '#define AppVersion\s+"([^"]+)"').Matches[0].Groups[1].Value
$build   = Join-Path $root "build\Geminus_artefacts\Release"
$stage   = Join-Path $env:TEMP ("002-zip-" + [guid]::NewGuid().ToString("N"))
$dist    = Join-Path $root "dist"

foreach ($d in "VST3\002 By SPKR", "CLAP", "Standalone") { New-Item -ItemType Directory -Force (Join-Path $stage $d) | Out-Null }
New-Item -ItemType Directory -Force $dist | Out-Null

Copy-Item -Recurse (Join-Path $build "VST3\002 by SPKR.vst3") (Join-Path $stage "VST3\002 By SPKR")
Copy-Item (Join-Path $build "CLAP\002 by SPKR.clap") (Join-Path $stage "CLAP")
Copy-Item (Join-Path $build "Standalone\002 by SPKR.exe") (Join-Path $stage "Standalone")
Copy-Item (Join-Path $root "installer\README.txt") $stage
Copy-Item (Join-Path $root "installer\LICENSE.txt") $stage
@"
Manual install
--------------
1. Copy the folder VST3\"002 By SPKR" into
   C:\Program Files\Common Files\VST3\
2. Optional: copy CLAP\"002 by SPKR.clap" into C:\Program Files\Common Files\CLAP\
3. Rescan plug-ins in your DAW.
The standalone app needs no install: run Standalone\"002 by SPKR.exe".
"@ | Set-Content (Join-Path $stage "INSTALL.txt") -Encoding UTF8

$zip = Join-Path $dist "002-by-SPKR-$version-Windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Remove-Item -Recurse -Force $stage
"zip: $zip"
