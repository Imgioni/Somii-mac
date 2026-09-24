Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$SRC = "C:\Users\w0nde\Desktop\Vst\resources"
$REF = "C:\Users\w0nde\Desktop\Vst\docs\reference\assets"
$OUT = "C:\Users\w0nde\Desktop\Vst\design"

function Load-Px($path) {
  $img = [System.Drawing.Bitmap]::FromFile($path)
  $w=$img.Width; $h=$img.Height
  $bd = $img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $stride=$bd.Stride; $bytes = New-Object byte[] ($stride*$h)
  [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$bytes,0,$bytes.Length)
  $img.UnlockBits($bd); $img.Dispose()
  @{ w=$w; h=$h; stride=$stride; b=$bytes }
}

# Build a 32bppArgb bitmap from a pixel map, optionally pulling a magenta key
function To-Bitmap($p, $x0, $y0, $cw, $ch, $chroma) {
  $bmp = New-Object System.Drawing.Bitmap($cw, $ch, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $bd = $bmp.LockBits((New-Object System.Drawing.Rectangle 0,0,$cw,$ch),
        [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $ostride = $bd.Stride
  $obytes = New-Object byte[] ($ostride*$ch)
  for ($y=0; $y -lt $ch; $y++) {
    $sy = $y0 + $y
    for ($x=0; $x -lt $cw; $x++) {
      $sx = $x0 + $x
      $si = $sy*$p.stride + $sx*4
      $oi = $y*$ostride + $x*4
      $b=$p.b[$si]; $g=$p.b[$si+1]; $r=$p.b[$si+2]; $a=$p.b[$si+3]
      if ($chroma) {
        # distance from key colour (249,3,251)
        $dr=$r-249; $dg=$g-3; $db=$b-251
        $d=[Math]::Sqrt($dr*$dr + $dg*$dg + $db*$db)
        if ($d -lt 70) { $a=0 }
        elseif ($d -lt 150) { $a=[int](255*(($d-70)/80.0)) }
        else { $a=255 }
        # de-spill: magenta fringe pushes B (and R) above G
        if ($a -gt 0 -and $b -gt $g -and $r -gt $g) { $b=$g }
      }
      $obytes[$oi]=$b; $obytes[$oi+1]=$g; $obytes[$oi+2]=$r; $obytes[$oi+3]=$a
    }
  }
  [System.Runtime.InteropServices.Marshal]::Copy($obytes,0,$bd.Scan0,$obytes.Length)
  $bmp.UnlockBits($bd)
  $bmp
}

function Save-Scaled($bmp, $outW, $outH, $name) {
  $dst = New-Object System.Drawing.Bitmap($outW, $outH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $gfx = [System.Drawing.Graphics]::FromImage($dst)
  $gfx.CompositingMode    = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $gfx.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $gfx.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $gfx.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $gfx.DrawImage($bmp, (New-Object System.Drawing.Rectangle 0,0,$outW,$outH))
  $gfx.Dispose()
  $path = Join-Path $OUT $name
  $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $dst.Dispose(); $bmp.Dispose()
  $kb = (Get-Item $path).Length/1KB
  "{0,-20} {1,4}x{2,-4} {3,7:N1} KB" -f $name, $outW, $outH, $kb
}

# tight bbox inside a region; mode 'alpha' or 'chroma'
function Get-BBox($p, $rx, $ry, $rw, $rh, $mode) {
  $xmin=999999; $xmax=-1; $ymin=999999; $ymax=-1
  for ($y=$ry; $y -lt ($ry+$rh); $y++) {
    for ($x=$rx; $x -lt ($rx+$rw); $x++) {
      $i=$y*$p.stride + $x*4
      $keep=$false
      if ($mode -eq "alpha") { $keep = ($p.b[$i+3] -gt 40) }
      else {
        $dr=$p.b[$i+2]-249; $dg=$p.b[$i+1]-3; $db=$p.b[$i]-251
        $keep = ([Math]::Sqrt($dr*$dr+$dg*$dg+$db*$db) -ge 110)
      }
      if ($keep) {
        if ($x -lt $xmin) {$xmin=$x}; if ($x -gt $xmax) {$xmax=$x}
        if ($y -lt $ymin) {$ymin=$y}; if ($y -gt $ymax) {$ymax=$y}
      }
    }
  }
  @{ x=$xmin; y=$ymin; w=($xmax-$xmin+1); h=($ymax-$ymin+1) }
}

"=== KNOBS (magenta key) ==="
$k = Load-Px "$SRC\knobs.png"
$names = @("knob-cream","knob-orange","knob-dark")
for ($i=0; $i -lt 3; $i++) {
  $rx = [int](1448/3*$i); $rw = [int](1448/3)
  $bb = Get-BBox $k $rx 0 $rw 1086 "chroma"
  # pad 3px, square it about the cap centre so rotation stays concentric
  $cx = $bb.x + $bb.w/2; $cy = $bb.y + $bb.h/2
  $side = [int]([Math]::Max($bb.w,$bb.h) + 6)
  $sx = [int]($cx - $side/2); $sy = [int]($cy - $side/2)
  if ($sx -lt 0) {$sx=0}; if ($sy -lt 0) {$sy=0}
  if ($sx+$side -gt 1448) {$side=1448-$sx}; if ($sy+$side -gt 1086) {$side=1086-$sy}
  "  {0,-12} bbox x={1} y={2} w={3} h={4}  -> square {5}x{5} at ({6},{7})" -f $names[$i],$bb.x,$bb.y,$bb.w,$bb.h,$side,$sx,$sy
  $bmp = To-Bitmap $k $sx $sy $side $side $true
  Save-Scaled $bmp 128 128 "$($names[$i]).png"
}

"=== FADERS ==="
$f = Load-Px "$SRC\faders.png"
$cells = @(
  @{n="fader-cap-grey";   x=274;  w=274},
  @{n="fader-cap-orange"; x=720;  w=280},
  @{n="fader-cap-dark";   x=1172; w=280},
  @{n="fader-slot";       x=1630; w=204}
)
foreach ($c in $cells) {
  $bb = Get-BBox $f $c.x 0 $c.w 724 "alpha"
  "  {0,-18} bbox x={1} y={2} w={3} h={4}" -f $c.n,$bb.x,$bb.y,$bb.w,$bb.h
  $bmp = To-Bitmap $f $bb.x $bb.y $bb.w $bb.h $false
  if ($c.n -eq "fader-slot") { Save-Scaled $bmp 44 260 "$($c.n).png" }
  else { Save-Scaled $bmp 56 110 "$($c.n).png" }
}

"=== BUTTONS + LEDS ==="
$bt = Load-Px "$SRC\buttons.png"
$bcells = @(
  @{n="btn-off"; x=454;  w=542; y=280; h=380},
  @{n="btn-on";  x=1174; w=542; y=280; h=380},
  @{n="led-off"; x=454;  w=542; y=40;  h=230},
  @{n="led-on";  x=1174; w=542; y=40;  h=230}
)
foreach ($c in $bcells) {
  $bb = Get-BBox $bt $c.x $c.y $c.w $c.h "alpha"
  "  {0,-10} bbox x={1} y={2} w={3} h={4}" -f $c.n,$bb.x,$bb.y,$bb.w,$bb.h
  $bmp = To-Bitmap $bt $bb.x $bb.y $bb.w $bb.h $false
  if ($c.n -like "led*") { Save-Scaled $bmp 40 40 "$($c.n).png" }
  else { Save-Scaled $bmp 120 84 "$($c.n).png" }
}

"=== RIBBON / CHEEK / OCTAVE ==="
$rb = Load-Px "$SRC\ribbon.png"
$bb = Get-BBox $rb 0 0 2172 724 "alpha"
"  ribbon bbox x={0} y={1} w={2} h={3}" -f $bb.x,$bb.y,$bb.w,$bb.h
Save-Scaled (To-Bitmap $rb $bb.x $bb.y $bb.w $bb.h $false) 1200 64 "ribbon.png"

$ch = Load-Px "$SRC\cheek.png"
$bb = Get-BBox $ch 0 0 724 2172 "alpha"
"  cheek bbox x={0} y={1} w={2} h={3}" -f $bb.x,$bb.y,$bb.w,$bb.h
Save-Scaled (To-Bitmap $ch $bb.x $bb.y $bb.w $bb.h $false) 56 900 "cheek.png"

$oc = Load-Px "$REF\03_octave.png"
$bb = Get-BBox $oc 0 0 1639 960 "alpha"
"  octave bbox x={0} y={1} w={2} h={3}" -f $bb.x,$bb.y,$bb.w,$bb.h
Save-Scaled (To-Bitmap $oc $bb.x $bb.y $bb.w $bb.h $false) 600 301 "octave.png"

"=== SURFACES ==="
$ps = Load-Px "$SRC\panel_surface.png"
Save-Scaled (To-Bitmap $ps 0 0 1254 1254 $false) 256 256 "panel.png"
$sh = Load-Px "$SRC\shading.png"
Save-Scaled (To-Bitmap $sh 0 0 1672 941 $false) 800 450 "shading.png"
$gl = Load-Px "$SRC\glass.png"
Save-Scaled (To-Bitmap $gl 0 0 1672 941 $false) 320 180 "glass.png"

""
"TOTAL: {0:N1} KB" -f ((Get-ChildItem "$OUT\*.png" | Measure-Object -Property Length -Sum).Sum/1KB)
