param([string]$Destination = '002-demo.zip', [string]$BuildDir = 'build-vs')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$projectRoot = Split-Path $PSScriptRoot -Parent
$release = Join-Path $projectRoot "$BuildDir/Geminus_artefacts/Release"
$env:GEMINUS_BUILD_DIR = $BuildDir
$archivePath = Join-Path $projectRoot $Destination
$archiveDirectory = Split-Path -Parent $archivePath
if (!(Test-Path -LiteralPath $archiveDirectory -PathType Container)) {
  New-Item -ItemType Directory -Path $archiveDirectory | Out-Null
}
if (Test-Path -LiteralPath $archivePath) { throw "Archive already exists: $archivePath" }
& node (Join-Path $PSScriptRoot 'check-ui-assets.mjs') --binaries
if ($LASTEXITCODE -ne 0) { throw 'Packaging stopped: at least one build is stale.' }
$files = [ordered]@{
  'VST3/002 By SPKR/002 by SPKR.vst3/Contents/x86_64-win/002 by SPKR.vst3' = Join-Path $release 'VST3/002 by SPKR.vst3/Contents/x86_64-win/002 by SPKR.vst3'
  'VST3/002 By SPKR/002 by SPKR.vst3/Contents/Resources/moduleinfo.json' = Join-Path $release 'VST3/002 by SPKR.vst3/Contents/Resources/moduleinfo.json'
  'CLAP/002 by SPKR.clap' = Join-Path $release 'CLAP/002 by SPKR.clap'
  'Standalone/002 by SPKR.exe' = Join-Path $release 'Standalone/002 by SPKR.exe'
  'DEMO_PACKAGE_README.txt' = Join-Path $PSScriptRoot 'demo-package-readme.txt'
}
foreach ($path in $files.Values) { if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing build: $path" } }
$checksums = foreach ($entry in $files.GetEnumerator()) {
  if ($entry.Key -match '\.(vst3|clap|exe)$') { "$( (Get-FileHash -LiteralPath $entry.Value -Algorithm SHA256).Hash.ToLower() )  $($entry.Key)" }
}
$zip = [IO.Compression.ZipFile]::Open($archivePath, [IO.Compression.ZipArchiveMode]::Create)
try {
  foreach ($entry in $files.GetEnumerator()) {
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $entry.Value, $entry.Key, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
  }
  $writer = [IO.StreamWriter]::new($zip.CreateEntry('SHA256SUMS.txt').Open())
  try { $writer.WriteLine($checksums -join "`n") } finally { $writer.Dispose() }
} finally { $zip.Dispose() }

# Read back every entry and compare it with the current build, not the old demo.
$zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
  foreach ($entry in $files.GetEnumerator()) {
    $stream = $zip.GetEntry($entry.Key).Open()
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
    finally { $stream.Dispose(); $sha.Dispose() }
    if ($actual -ne (Get-FileHash -LiteralPath $entry.Value -Algorithm SHA256).Hash) { throw "Archive verification failed: $($entry.Key)" }
  }
} finally { $zip.Dispose() }
Get-Item -LiteralPath $archivePath | Select-Object FullName, Length
Write-Output 'Verified all archived files against the current builds.'
