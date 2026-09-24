Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

$img=[System.Drawing.Bitmap]::FromFile("$REF\knobs2.png")
$w=$img.Width;$h=$img.Height
$bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$st=$bd.Stride;$by=New-Object byte[] ($st*$h)
[System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length);$img.UnlockBits($bd);$img.Dispose()

# Crop each cap square about the TOP-FACE centre rather than the whole body.
# Trims most of the knurled skirt (it was ~18% of the render height, which is
# what read as "thick") and puts the rotation axis on the cap axis.
$FACE = 0.82   # top face occupies this fraction of the bbox height
$names=@("k2-cream","k2-orange","k2-dark")
for($i=0;$i -lt 3;$i++){
  $rx=[int]($w/3*$i); $rw=[int]($w/3)
  $xmin=999999;$xmax=-1;$ymin=999999;$ymax=-1
  for($y=0;$y -lt $h;$y++){for($x=$rx;$x -lt ($rx+$rw);$x++){
    $ii=$y*$st+$x*4
    if($by[$ii+3] -gt 40){if($x -lt $xmin){$xmin=$x};if($x -gt $xmax){$xmax=$x};if($y -lt $ymin){$ymin=$y};if($y -gt $ymax){$ymax=$y}}}}
  $bw=$xmax-$xmin+1; $bh=$ymax-$ymin+1
  $faceBottom = $ymin + $FACE*$bh
  $fcy = ($ymin + $faceBottom)/2
  $fcx = ($xmin + $xmax)/2
  $side = [int]($bw + 6)
  $sx=[int]($fcx - $side/2); $sy=[int]($fcy - $side/2)
  $src=New-Object System.Drawing.Bitmap($side,$side,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g2=[System.Drawing.Graphics]::FromImage($src)
  $g2.Clear([System.Drawing.Color]::Transparent)
  $g2.DrawImage([System.Drawing.Image]::FromFile("$REF\knobs2.png"),
    (New-Object System.Drawing.Rectangle 0,0,$side,$side), $sx,$sy,$side,$side,[System.Drawing.GraphicsUnit]::Pixel)
  $g2.Dispose()
  $dst=New-Object System.Drawing.Bitmap(132,132,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g3=[System.Drawing.Graphics]::FromImage($dst)
  $g3.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g3.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g3.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g3.DrawImage($src,(New-Object System.Drawing.Rectangle 0,0,132,132))
  $g3.Dispose();$src.Dispose()
  $dst.Save((Join-Path $OUT "$($names[$i]).png"),[System.Drawing.Imaging.ImageFormat]::Png);$dst.Dispose()
  "  {0,-10} face centre ({1:N0},{2:N0}) side {3} -> 132x132, skirt trimmed {4:N0}px, {5:N1} KB" -f `
    $names[$i],$fcx,$fcy,$side,($ymax-($sy+$side)),((Get-Item (Join-Path $OUT "$($names[$i]).png")).Length/1KB)
}
