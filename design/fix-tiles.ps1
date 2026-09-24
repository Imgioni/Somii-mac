Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"
$SRC="C:\Users\w0nde\Desktop\Vst\resources"; $REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

"=== cheek: which source is light? (mean luminance of opaque pixels) ==="
foreach ($f in @("$SRC\cheek.png","$REF\06_cheek.png","$REF\06_cheek_gray.png")) {
  $img=[System.Drawing.Bitmap]::FromFile($f); $w=$img.Width; $h=$img.Height
  $bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $st=$bd.Stride; $by=New-Object byte[] ($st*$h)
  [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length); $img.UnlockBits($bd); $img.Dispose()
  $sum=0.0; $n=0
  for($y=0;$y -lt $h;$y+=4){for($x=0;$x -lt $w;$x+=4){$i=$y*$st+$x*4
    if($by[$i+3] -gt 200){$sum += (0.299*$by[$i+2]+0.587*$by[$i+1]+0.114*$by[$i]); $n++}}}
  "  {0,-46} {1,4}x{2,-5} mean lum {3,6:N1} over {4} px" -f (Split-Path $f -Leaf),$w,$h,($sum/[Math]::Max($n,1)),$n
}

"=== panel: flatten gradient, then mirror into a seamless tile ==="
$src=[System.Drawing.Bitmap]::FromFile("$SRC\panel_surface.png")
$q=128
$small=New-Object System.Drawing.Bitmap($q,$q,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($small)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($src,0,0,$q,$q); $g.Dispose(); $src.Dispose()

# read, high-pass to kill the lighting gradient so mirrored quadrants don't quilt
$bd=$small.LockBits((New-Object System.Drawing.Rectangle 0,0,$q,$q),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$st=$bd.Stride; $by=New-Object byte[] ($st*$q)
[System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length); $small.UnlockBits($bd); $small.Dispose()
$lum=New-Object double[] ($q*$q)
for($y=0;$y -lt $q;$y++){for($x=0;$x -lt $q;$x++){$i=$y*$st+$x*4
  $lum[$y*$q+$x]=0.299*$by[$i+2]+0.587*$by[$i+1]+0.114*$by[$i]}}
$mean=($lum | Measure-Object -Average).Average
# box blur radius 16 = the lighting gradient
$rad=16; $low=New-Object double[] ($q*$q)
for($y=0;$y -lt $q;$y++){for($x=0;$x -lt $q;$x++){
  $s=0.0;$c=0
  for($dy=-$rad;$dy -le $rad;$dy+=4){for($dx=-$rad;$dx -le $rad;$dx+=4){
    $yy=[Math]::Min([Math]::Max($y+$dy,0),$q-1); $xx=[Math]::Min([Math]::Max($x+$dx,0),$q-1)
    $s+=$lum[$yy*$q+$xx];$c++}}
  $low[$y*$q+$x]=$s/$c}}
$flat=New-Object System.Drawing.Bitmap($q,$q,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
for($y=0;$y -lt $q;$y++){for($x=0;$x -lt $q;$x++){
  $d=$lum[$y*$q+$x]-$low[$y*$q+$x]
  $v=[int][Math]::Round($mean+$d)
  if($v -lt 0){$v=0}; if($v -gt 255){$v=255}
  $flat.SetPixel($x,$y,[System.Drawing.Color]::FromArgb($v,$v,$v))}}

# mirror into 2x2 -> guaranteed seamless
$tile=New-Object System.Drawing.Bitmap(($q*2),($q*2),[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($tile)
$g.DrawImage($flat,0,0,$q,$q)
$fh=$flat.Clone(); $fh.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX); $g.DrawImage($fh,$q,0,$q,$q)
$fv=$flat.Clone(); $fv.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY); $g.DrawImage($fv,0,$q,$q,$q)
$fb=$flat.Clone(); $fb.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY); $g.DrawImage($fb,$q,$q,$q,$q)
$g.Dispose(); $flat.Dispose();$fh.Dispose();$fv.Dispose();$fb.Dispose()
$tile.Save("$OUT\panel.png",[System.Drawing.Imaging.ImageFormat]::Png); $tile.Dispose()
"  panel.png -> {0}x{0} seamless, {1:N1} KB" -f ($q*2), ((Get-Item "$OUT\panel.png").Length/1KB)

"=== octave: trim end-faces so tiles butt cleanly ==="
$oc=[System.Drawing.Bitmap]::FromFile("$REF\03_octave.png")
# content was x 82..1567 ; shave the finished side faces off both ends
$x0=82+14; $x1=1567-14; $wid=$x1-$x0+1
$crop=New-Object System.Drawing.Bitmap($wid,747,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[System.Drawing.Graphics]::FromImage($crop)
$g.DrawImage($oc,(New-Object System.Drawing.Rectangle 0,0,$wid,747),$x0,99,$wid,747,[System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $oc.Dispose()
$dst=New-Object System.Drawing.Bitmap(460,231,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[System.Drawing.Graphics]::FromImage($dst)
$g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($crop,(New-Object System.Drawing.Rectangle 0,0,460,231)); $g.Dispose(); $crop.Dispose()
$dst.Save("$OUT\octave.png",[System.Drawing.Imaging.ImageFormat]::Png); $dst.Dispose()
"  octave.png -> 460x231, {0:N1} KB" -f ((Get-Item "$OUT\octave.png").Length/1KB)

"=== tiling test sheet ==="
$W=1200;$H=460
$sheet=New-Object System.Drawing.Bitmap($W,$H,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($sheet)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$pt=[System.Drawing.Bitmap]::FromFile("$OUT\panel.png")
for($y=0;$y -lt 260;$y+=128){for($x=0;$x -lt $W;$x+=128){$g.DrawImage($pt,$x,$y,128,128)}}
$pt.Dispose()
$ot=[System.Drawing.Bitmap]::FromFile("$OUT\octave.png")
for($i=0;$i -lt 5;$i++){ $g.DrawImage($ot, (40+$i*224), 280, 224, 112) }
$ot.Dispose()
$font=New-Object System.Drawing.Font("Segoe UI",12,[System.Drawing.FontStyle]::Bold)
$g.DrawString("panel tiled 128px - look for seams",$font,[System.Drawing.Brushes]::Black,12,8)
$g.DrawString("octave tiled x5 (= 35 white keys)",$font,[System.Drawing.Brushes]::Black,40,400)
$g.Dispose()
$sheet.Save("$OUT\_tile_test.png",[System.Drawing.Imaging.ImageFormat]::Png); $sheet.Dispose()
"  written"
