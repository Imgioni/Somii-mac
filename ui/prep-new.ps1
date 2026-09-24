Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\ui"

# One pass over the raw bytes, no per-pixel function calls (those made this minutes-slow).
function Cells($file,$expected,$lum){
  $path=Join-Path $REF $file
  $img=[System.Drawing.Bitmap]::FromFile($path); $w=$img.Width; $h=$img.Height
  $bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $st=$bd.Stride; $by=New-Object byte[] ($st*$h)
  [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length); $img.UnlockBits($bd); $img.Dispose()

  $colCount=New-Object int[] $w
  $rowTop=New-Object int[] $w; $rowBot=New-Object int[] $w
  for($x=0;$x -lt $w;$x++){ $rowTop[$x]=$h; $rowBot[$x]=-1 }
  for($y=0;$y -lt $h;$y++){
    $base=$y*$st
    for($x=0;$x -lt $w;$x++){
      $i=$base+$x*4
      $on = if($lum){ (77*$by[$i+2]+150*$by[$i+1]+29*$by[$i]) -gt 6656 } else { $by[$i+3] -gt 40 }
      if($on){ $colCount[$x]++; if($y -lt $rowTop[$x]){$rowTop[$x]=$y}; if($y -gt $rowBot[$x]){$rowBot[$x]=$y} }
    }
  }
  $runs=New-Object System.Collections.ArrayList
  $x=0
  while($x -lt $w){
    if($colCount[$x] -le 3){$x++;continue}
    $x0=$x; $y0=$h; $y1=-1
    while($x -lt $w -and $colCount[$x] -gt 3){
      if($rowTop[$x] -lt $y0){$y0=$rowTop[$x]}; if($rowBot[$x] -gt $y1){$y1=$rowBot[$x]}; $x++
    }
    [void]$runs.Add(@{x=$x0;y=$y0;w=($x-$x0);h=($y1-$y0+1);src=$path})
  }
  if($expected -gt 0 -and $runs.Count -ne $expected){ Write-Host ("  !! {0}: {1} cells, expected {2}" -f $file,$runs.Count,$expected) }
  return ,$runs
}
function Cut($r,$ow,$name){
  $oh=[int][Math]::Round([double]$ow*$r.h/$r.w)
  $img=[System.Drawing.Bitmap]::FromFile($r.src)
  $dst=New-Object System.Drawing.Bitmap($ow,$oh,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g=[System.Drawing.Graphics]::FromImage($dst)
  $g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($img,(New-Object System.Drawing.Rectangle 0,0,$ow,$oh),$r.x,$r.y,$r.w,$r.h,[System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose();$img.Dispose()
  $dst.Save((Join-Path $OUT $name),[System.Drawing.Imaging.ImageFormat]::Png);$dst.Dispose()
  "  {0,-18} src {1,4}x{2,-4} -> {3,3}x{4,-4} {5,5:N0} KB" -f $name,$r.w,$r.h,$ow,$oh,((Get-Item (Join-Path $OUT $name)).Length/1KB)
}

$jobs=@(
 @{f="13_bender_lever.png";     n=@("bender");      w=120; lum=$false},
 @{f="14_single_white_key.png"; n=@("whitekey");    w=80;  lum=$false},
 @{f="15_fader_track.png";      n=@("fader-track"); w=40;  lum=$false},
 @{f="17_tick_ladder.png";      n=@("ticks");       w=40;  lum=$false},
 @{f="16_fader_caps.png";       n=@("fader-cap-grey","fader-cap-orange","fader-cap-dark"); w=64; lum=$true}
)
foreach($j in $jobs){
  $c=Cells $j.f $j.n.Count $j.lum
  for($i=0;$i -lt [Math]::Min($j.n.Count,$c.Count);$i++){ Cut $c[$i] $j.w "$($j.n[$i]).png" }
}
