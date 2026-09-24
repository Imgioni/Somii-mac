Add-Type -AssemblyName System.Drawing
$path = Join-Path (Get-Location) 'ui\btn-dark.png'
$src = [System.Drawing.Bitmap]::FromFile($path)
$out = New-Object System.Drawing.Bitmap($src.Width, $src.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for ($y = 0; $y -lt $src.Height; $y++) {
  for ($x = 0; $x -lt $src.Width; $x++) {
    $c = $src.GetPixel($x, $y)
    # The source crop contains a red neighboring knob in the upper-left corner.
    # Dark button pixels are neutral; remove only strongly chromatic red contamination.
    if ($c.A -gt 0 -and $c.R -gt $c.G + 12 -and $c.R -gt $c.B + 12) {
      $out.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    } else {
      $out.SetPixel($x, $y, $c)
    }
  }
}
$tmp = $path + '.tmp.png'
$out.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
$out.Dispose(); $src.Dispose()
Move-Item -LiteralPath $tmp -Destination $path -Force
