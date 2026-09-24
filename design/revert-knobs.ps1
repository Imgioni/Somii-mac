Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"
$img=[System.Drawing.Bitmap]::FromFile("$REF\knobs2.png")
$w=$img.Width;$h=$img.Height
$bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$st=$bd.Stride;$by=New-Object byte[] ($st*$h)
[System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length);$img.UnlockBits($bd);$img.Dispose()
$names=@("k2-cream","k2-orange","k2-dark")
for($i=0;$i -lt 3;$i++){
  $rx=[int]($w/3*$i); $rw=[int]($w/3)
  $xmin=999999;$xmax=-1;$ymin=999999;$ymax=-1
  for($y=0;$y -lt $h;$y++){for($x=$rx;$x -lt ($rx+$rw);$x++){
    $ii=$y*$st+$x*4
    if($by[$ii+3] -gt 40){if($x -lt $xmin){$xmin=$x};if($x -gt $xmax){$xmax=$x};if($y -lt $ymin){$ymin=$y};if($y -gt $ymax){$ymax=$y}}}}
  $bw=$xmax-$xmin+1; $bh=$ymax-$ymin+1
  $cx=$xmin+$bw/2; $cy=$ymin+$bh/2
  $side=[int]([Math]::Max($bw,$bh)+8)
  $sx=[int]($cx-$side/2); $sy=[int]($cy-$side/2)
  $src=New-Object System.Drawing.Bitmap($side,$side,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g2=[System.Drawing.Graphics]::FromImage($src); $g2.Clear([System.Drawing.Color]::Transparent)
  $g2.DrawImage([System.Drawing.Image]::FromFile("$REF\knobs2.png"),
    (New-Object System.Drawing.Rectangle 0,0,$side,$side),$sx,$sy,$side,$side,[System.Drawing.GraphicsUnit]::Pixel)
  $g2.Dispose()
  $dst=New-Object System.Drawing.Bitmap(132,132,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g3=[System.Drawing.Graphics]::FromImage($dst)
  $g3.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g3.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g3.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g3.DrawImage($src,(New-Object System.Drawing.Rectangle 0,0,132,132))
  $g3.Dispose();$src.Dispose()
  $dst.Save((Join-Path $OUT "$($names[$i]).png"),[System.Drawing.Imaging.ImageFormat]::Png);$dst.Dispose()
  "  {0,-10} reverted: whole-body square {1}, {2:N1} KB" -f $names[$i],$side,((Get-Item (Join-Path $OUT "$($names[$i]).png")).Length/1KB)
}
