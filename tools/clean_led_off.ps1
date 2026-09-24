Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$path = Join-Path $root 'ui\led-off.png'
$backup = Join-Path $root 'ui\led-off.png.before-clean'
if (-not (Test-Path -LiteralPath $backup)) { Copy-Item -LiteralPath $path -Destination $backup }

$src = [System.Drawing.Bitmap]::new($path)
$dst = [System.Drawing.Bitmap]::new($src.Width, $src.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$cx = ($src.Width - 1) / 2.0
$cy = ($src.Height - 1) / 2.0
$radius = 16.5
for ($y = 0; $y -lt $src.Height; $y++) {
  for ($x = 0; $x -lt $src.Width; $x++) {
    $dx = $x - $cx; $dy = $y - $cy
    $r = [Math]::Sqrt($dx * $dx + $dy * $dy)
    if ($r -le ($radius - 1.0)) {
      $c = $src.GetPixel($x, $y)
      # The source crop contains a gray/black neighboring edge around the
      # red LED. Keep the red LED core only; that edge becomes a dash at 10px.
      if ($c.A -gt 0 -and $c.R -gt ($c.G + 2) -and $c.R -gt ($c.B + 2)) {
        $dst.SetPixel($x, $y, $c)
      } else {
        $dst.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
      }
    } else {
      $dst.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    }
  }
}
$src.Dispose()
$dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$dst.Dispose()
Write-Output "Cleaned circular led-off crop: $path"
