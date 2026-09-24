Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"
$OUT = "C:\Users\w0nde\Desktop\Vst\design"

function Reload-Save($name, $w, $h, $asJpeg) {
  $p = Join-Path $OUT $name
  $src = [System.Drawing.Bitmap]::FromFile($p)
  $dst = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($dst)
  $g.CompositingMode   = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0,0,$w,$h))
  $g.Dispose(); $src.Dispose()
  if ($asJpeg) {
    $flat = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g2 = [System.Drawing.Graphics]::FromImage($flat)
    $g2.DrawImage($dst, 0, 0, $w, $h); $g2.Dispose()
    $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
    $pars = New-Object System.Drawing.Imaging.EncoderParameters(1)
    $pars.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, 82)
    $newp = $p -replace '\.png$', '.jpg'
    $flat.Save($newp, $enc, $pars); $flat.Dispose(); $dst.Dispose()
    Remove-Item $p
    "{0,-18} -> {1,4}x{2,-4} {3,7:N1} KB (jpg)" -f $name, $w, $h, ((Get-Item $newp).Length/1KB)
  } else {
    $dst.Save($p, [System.Drawing.Imaging.ImageFormat]::Png); $dst.Dispose()
    "{0,-18} -> {1,4}x{2,-4} {3,7:N1} KB" -f $name, $w, $h, ((Get-Item $p).Length/1KB)
  }
}

"=== trimming oversized entries ==="
Reload-Save "shading.png" 400 225 $true
Reload-Save "octave.png"  460 231 $false
Reload-Save "ribbon.png"  900 48  $false
Reload-Save "panel.png"   200 200 $false
Reload-Save "glass.png"   240 135 $false

"=== contact sheet on panel-grey ==="
$W=1180; $H=620
$sheet = New-Object System.Drawing.Bitmap($W,$H,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
# tile the real panel texture as the ground, so we see the sprites in context
$tile = [System.Drawing.Bitmap]::FromFile((Join-Path $OUT "panel.png"))
for ($y=0; $y -lt $H; $y+=200) { for ($x=0; $x -lt $W; $x+=200) { $g.DrawImage($tile,$x,$y,200,200) } }
$tile.Dispose()
$font  = New-Object System.Drawing.Font("Segoe UI",11,[System.Drawing.FontStyle]::Bold)
$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(40,40,40))

function Place($name,$x,$y,$w,$h,$label,$rot) {
  $b = [System.Drawing.Bitmap]::FromFile((Join-Path $OUT $name))
  $st = $g.Save()
  if ($rot -ne 0) {
    $g.TranslateTransform($x + $w/2, $y + $h/2)
    $g.RotateTransform($rot)
    $g.TranslateTransform(-($w/2), -($h/2))
    $g.DrawImage($b, 0, 0, $w, $h)
  } else { $g.DrawImage($b, $x, $y, $w, $h) }
  $g.Restore($st)
  $g.DrawString($label, $font, $brush, $x, $y + $h + 4)
  $b.Dispose()
}

# knobs at three rotations each - this is the value-indication test
Place "knob-cream.png"   40  30 96 96 "cream -150" -150
Place "knob-cream.png"  170  30 96 96 "cream 0"       0
Place "knob-cream.png"  300  30 96 96 "cream +150"  150
Place "knob-orange.png" 440  30 96 96 "orange -150" -150
Place "knob-orange.png" 570  30 96 96 "orange 0"      0
Place "knob-orange.png" 700  30 96 96 "orange +150" 150
Place "knob-dark.png"   840  30 96 96 "dark -150"  -150
Place "knob-dark.png"   970  30 96 96 "dark 0"        0
Place "knob-dark.png"  1070  30 96 96 "dark +150"   150

Place "fader-slot.png"        40 190 30 180 "slot"     0
Place "fader-cap-grey.png"   110 190 38 75  "cap grey"   0
Place "fader-cap-orange.png" 180 190 38 75  "cap orange" 0
Place "fader-cap-dark.png"   250 190 38 75  "cap dark"   0
Place "btn-off.png"          330 190 76 53  "btn off"    0
Place "btn-on.png"           430 190 76 53  "btn on"     0
Place "led-off.png"          540 190 26 26  "led off"    0
Place "led-on.png"           590 190 26 26  "led on"     0
Place "glass.png"            650 190 180 101 "glass"     0
Place "cheek.png"            870 190 24 200 "cheek"      0
Place "octave.png"           920 190 230 116 "octave"    0
Place "ribbon.png"            40 420 700 37 "ribbon"     0

$g.Dispose()
$sheet.Save((Join-Path $OUT "_contact_sheet.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()
""
"contact sheet written"
"TOTAL embed payload: {0:N1} KB" -f ((Get-ChildItem "$OUT\*.png","$OUT\*.jpg" -Exclude "_contact_sheet.png" | Measure-Object -Property Length -Sum).Sum/1KB)
