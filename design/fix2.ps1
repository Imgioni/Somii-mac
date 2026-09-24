Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$SRC="C:\Users\w0nde\Desktop\Vst\resources"; $REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

function Load-Px($path){
  $img=[System.Drawing.Bitmap]::FromFile($path);$w=$img.Width;$h=$img.Height
  $bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $st=$bd.Stride;$by=New-Object byte[] ($st*$h)
  [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length);$img.UnlockBits($bd);$img.Dispose()
  @{w=$w;h=$h;stride=$st;b=$by}
}

"=== PANEL: centre crop (even lighting) + 2x2 mirror, no high-pass ==="
$src=[System.Drawing.Bitmap]::FromFile("$SRC\panel_surface.png")
$q=128
# take a 480x480 patch from the middle, where the product-shot lighting is flat
$patch=New-Object System.Drawing.Bitmap($q,$q,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($patch)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($src,(New-Object System.Drawing.Rectangle 0,0,$q,$q),387,387,480,480,[System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose();$src.Dispose()
$tile=New-Object System.Drawing.Bitmap(($q*2),($q*2),[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($tile)
$g.DrawImage($patch,0,0,$q,$q)
$a=$patch.Clone();$a.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX);$g.DrawImage($a,$q,0,$q,$q)
$b=$patch.Clone();$b.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY);$g.DrawImage($b,0,$q,$q,$q)
$c=$patch.Clone();$c.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY);$g.DrawImage($c,$q,$q,$q,$q)
$g.Dispose();$patch.Dispose();$a.Dispose();$b.Dispose();$c.Dispose()
$tile.Save("$OUT\panel.png",[System.Drawing.Imaging.ImageFormat]::Png);$tile.Dispose()
"  panel.png -> 256x256 mirrored, {0:N1} KB" -f ((Get-Item "$OUT\panel.png").Length/1KB)

"=== OCTAVE: measure white-key pitch, cut exactly one period ==="
$p=Load-Px "$REF\03_octave.png"
# white-key band sits below the black keys; content rows were 99..845
$scanY=[int](99 + 0.86*(845-99))
$isKey=New-Object bool[] $p.w
for($x=0;$x -lt $p.w;$x++){
  $i=$scanY*$p.stride+$x*4
  $lum=0.299*$p.b[$i+2]+0.587*$p.b[$i+1]+0.114*$p.b[$i]
  $isKey[$x] = ($p.b[$i+3] -gt 60 -and $lum -gt 140)
}
# gaps = dark separators between white keys
$gaps=@();$st=-1
for($x=0;$x -lt $p.w;$x++){
  if(-not $isKey[$x] -and $st -lt 0){$st=$x}
  elseif($isKey[$x] -and $st -ge 0){ if(($x-$st) -ge 2 -and ($x-$st) -le 40){$gaps += [int](($st+$x)/2)}; $st=-1 }
}
"  scan row y=$scanY ; separator centres: $($gaps -join ', ')"
if($gaps.Count -ge 6){
  $s1=$gaps[0]; $s6=$gaps[$gaps.Count-1]
  $pitch=($s6-$s1)/($gaps.Count-1)
  $x0=[int][Math]::Round($s1-$pitch)
  $wid=[int][Math]::Round($pitch*7)
  "  {0} separators, pitch={1:N2}px  => tile x={2} w={3} (one octave)" -f $gaps.Count,$pitch,$x0,$wid
  $oc=[System.Drawing.Bitmap]::FromFile("$REF\03_octave.png")
  $dst=New-Object System.Drawing.Bitmap(462,233,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g=[System.Drawing.Graphics]::FromImage($dst)
  $g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($oc,(New-Object System.Drawing.Rectangle 0,0,462,233),$x0,99,$wid,747,[System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose();$oc.Dispose()
  $dst.Save("$OUT\octave.png",[System.Drawing.Imaging.ImageFormat]::Png);$dst.Dispose()
  "  octave.png -> 462x233, {0:N1} KB" -f ((Get-Item "$OUT\octave.png").Length/1KB)
} else { "  !! only $($gaps.Count) separators found - not cutting" }

"=== retest ==="
$W=1200;$H=420
$sheet=New-Object System.Drawing.Bitmap($W,$H,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($sheet)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$pt=[System.Drawing.Bitmap]::FromFile("$OUT\panel.png")
for($y=0;$y -lt 230;$y+=115){for($x=0;$x -lt $W;$x+=115){$g.DrawImage($pt,$x,$y,115,115)}}
$pt.Dispose()
$ot=[System.Drawing.Bitmap]::FromFile("$OUT\octave.png")
for($i=0;$i -lt 5;$i++){ $g.DrawImage($ot,(30+$i*224),250,224,113) }
$g.DrawImage($ot,(30+5*224),250,32,113)   # top C
$ot.Dispose()
$ck=[System.Drawing.Bitmap]::FromFile("$OUT\cheek.png")
$g.DrawImage($ck,1160,250,26,150); $ck.Dispose()
$f=New-Object System.Drawing.Font("Segoe UI",12,[System.Drawing.FontStyle]::Bold)
$g.DrawString("panel tiled 115px",$f,[System.Drawing.Brushes]::Black,10,6)
$g.DrawString("61-key bed: octave x5 + top C          cheek ->",$f,[System.Drawing.Brushes]::Black,30,372)
$g.Dispose();$sheet.Save("$OUT\_tile_test.png",[System.Drawing.Imaging.ImageFormat]::Png);$sheet.Dispose()
"  written"
