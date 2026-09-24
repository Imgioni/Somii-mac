Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

function Load-Px($path){
  $img=[System.Drawing.Bitmap]::FromFile($path);$w=$img.Width;$h=$img.Height
  $bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $st=$bd.Stride;$by=New-Object byte[] ($st*$h)
  [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length);$img.UnlockBits($bd);$img.Dispose()
  @{w=$w;h=$h;stride=$st;b=$by}
}
function Corner($p,$name){
  $i=2*$p.stride+2*4
  "  {0,-14} {1,4}x{2,-5} corner R={3,3} G={4,3} B={5,3} A={6,3}" -f $name,$p.w,$p.h,$p.b[$i+2],$p.b[$i+1],$p.b[$i],$p.b[$i+3]
}
function BBox($p,$rx,$ry,$rw,$rh){
  $xmin=999999;$xmax=-1;$ymin=999999;$ymax=-1
  for($y=$ry;$y -lt ($ry+$rh);$y++){for($x=$rx;$x -lt ($rx+$rw);$x++){
    $i=$y*$p.stride+$x*4
    if($p.b[$i+3] -gt 40){
      if($x -lt $xmin){$xmin=$x};if($x -gt $xmax){$xmax=$x}
      if($y -lt $ymin){$ymin=$y};if($y -gt $ymax){$ymax=$y}}}}
  @{x=$xmin;y=$ymin;w=($xmax-$xmin+1);h=($ymax-$ymin+1)}
}
function Cut($srcPath,$sx,$sy,$sw,$sh,$ow,$oh,$name){
  $img=[System.Drawing.Bitmap]::FromFile($srcPath)
  $dst=New-Object System.Drawing.Bitmap($ow,$oh,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g=[System.Drawing.Graphics]::FromImage($dst)
  $g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($img,(New-Object System.Drawing.Rectangle 0,0,$ow,$oh),$sx,$sy,$sw,$sh,[System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose();$img.Dispose()
  $dst.Save((Join-Path $OUT $name),[System.Drawing.Imaging.ImageFormat]::Png);$dst.Dispose()
  "  {0,-20} {1,4}x{2,-4} {3,7:N1} KB" -f $name,$ow,$oh,((Get-Item (Join-Path $OUT $name)).Length/1KB)
}

"=== backgrounds ==="
$k=Load-Px "$REF\knobs2.png";   Corner $k "knobs2"
$s=Load-Px "$REF\switch3.png";  Corner $s "switch3"
$o=Load-Px "$REF\octave2.png";  Corner $o "octave2"

"=== KNOBS2: three caps, squared about each cap centre ==="
$names=@("k2-cream","k2-orange","k2-dark")
for($i=0;$i -lt 3;$i++){
  $rx=[int]($k.w/3*$i); $rw=[int]($k.w/3)
  $bb=BBox $k $rx 0 $rw $k.h
  $cx=$bb.x+$bb.w/2; $cy=$bb.y+$bb.h/2
  $side=[int]([Math]::Max($bb.w,$bb.h)+8)
  $sx=[int]($cx-$side/2); $sy=[int]($cy-$side/2)
  if($sx -lt 0){$sx=0}; if($sy -lt 0){$sy=0}
  if($sx+$side -gt $k.w){$side=$k.w-$sx}; if($sy+$side -gt $k.h){$side=$k.h-$sy}
  "  {0,-10} bbox {1}x{2} at ({3},{4}) -> square {5}" -f $names[$i],$bb.w,$bb.h,$bb.x,$bb.y,$side
  Cut "$REF\knobs2.png" $sx $sy $side $side 132 132 "$($names[$i]).png"
}

"=== SWITCH3: three detent states ==="
$snames=@("sw-top","sw-mid","sw-bot")
for($i=0;$i -lt 3;$i++){
  $rx=[int]($s.w/3*$i); $rw=[int]($s.w/3)
  $bb=BBox $s $rx 0 $rw $s.h
  "  {0,-8} bbox {1}x{2} at ({3},{4})" -f $snames[$i],$bb.w,$bb.h,$bb.x,$bb.y
  Cut "$REF\switch3.png" $bb.x $bb.y $bb.w $bb.h 44 96 "$($snames[$i]).png"
}

"=== OCTAVE2: measure pitch, cut exactly one period ==="
$bb=BBox $o 0 0 $o.w $o.h
"  content bbox x=$($bb.x) y=$($bb.y) w=$($bb.w) h=$($bb.h)"
$scanY=[int]($bb.y + 0.88*$bb.h)
$isKey=New-Object bool[] $o.w
for($x=0;$x -lt $o.w;$x++){
  $i=$scanY*$o.stride+$x*4
  $lum=0.299*$o.b[$i+2]+0.587*$o.b[$i+1]+0.114*$o.b[$i]
  $isKey[$x]=($o.b[$i+3] -gt 60 -and $lum -gt 140)
}
$gaps=@();$st=-1
for($x=0;$x -lt $o.w;$x++){
  if(-not $isKey[$x] -and $st -lt 0){$st=$x}
  elseif($isKey[$x] -and $st -ge 0){ if(($x-$st) -ge 1 -and ($x-$st) -le 46){$gaps+=[int](($st+$x)/2)}; $st=-1 }
}
"  scan y=$scanY  separators: $($gaps -join ', ')"
if($gaps.Count -ge 6){
  $pitch=($gaps[$gaps.Count-1]-$gaps[0])/($gaps.Count-1)
  $x0=[int][Math]::Round($gaps[0]-$pitch)
  $wid=[int][Math]::Round($pitch*7)
  $oh=[int][Math]::Round(233.0*$bb.h/$wid*$wid/233.0)  # keep true aspect below
  $outW=462; $outH=[int][Math]::Round(462.0*$bb.h/$wid)
  "  {0} separators, pitch={1:N2} -> tile x={2} w={3} h={4} (aspect {5:N3})" -f $gaps.Count,$pitch,$x0,$wid,$bb.h,($wid/$bb.h)
  Cut "$REF\octave2.png" $x0 $bb.y $wid $bb.h $outW $outH "octave.png"
} else { "  !! only $($gaps.Count) separators - not cutting" }
