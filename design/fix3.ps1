Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

# ---------- dark panel tile from 02_panel_surface_gray.png ----------
$bmp=[System.Drawing.Bitmap]::FromFile("$REF\02_panel_surface_gray.png")
$q=128
$patch=New-Object System.Drawing.Bitmap($q,$q,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$gfx=[System.Drawing.Graphics]::FromImage($patch)
$gfx.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gfx.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gfx.DrawImage($bmp,(New-Object System.Drawing.Rectangle 0,0,$q,$q),387,387,480,480,[System.Drawing.GraphicsUnit]::Pixel)
$gfx.Dispose();$bmp.Dispose()
$tile=New-Object System.Drawing.Bitmap(($q*2),($q*2),[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$gfx=[System.Drawing.Graphics]::FromImage($tile)
$gfx.DrawImage($patch,0,0,$q,$q)
$a=$patch.Clone();$a.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX);$gfx.DrawImage($a,$q,0,$q,$q)
$b=$patch.Clone();$b.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY);$gfx.DrawImage($b,0,$q,$q,$q)
$c=$patch.Clone();$c.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY);$gfx.DrawImage($c,$q,$q,$q,$q)
$gfx.Dispose();$patch.Dispose();$a.Dispose();$b.Dispose();$c.Dispose()
$tile.Save("$OUT\panel-dark.png",[System.Drawing.Imaging.ImageFormat]::Png)
$sr=0.0;$sg=0.0;$sb=0.0;$n=0
for($y=0;$y -lt 256;$y+=4){for($x=0;$x -lt 256;$x+=4){$p=$tile.GetPixel($x,$y);$sr+=$p.R;$sg+=$p.G;$sb+=$p.B;$n++}}
"panel-dark.png -> 256x256 seamless, base #{0:X2}{1:X2}{2:X2}, {3:N1} KB" -f [int]($sr/$n),[int]($sg/$n),[int]($sb/$n),((Get-Item "$OUT\panel-dark.png").Length/1KB)
$tile.Dispose()

# ---------- find the cap face / skirt boundary in knobs2 ----------
$img=[System.Drawing.Bitmap]::FromFile("$REF\knobs2.png")
$w=$img.Width;$h=$img.Height
$bd=$img.LockBits((New-Object System.Drawing.Rectangle 0,0,$w,$h),[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$st=$bd.Stride;$by=New-Object byte[] ($st*$h)
[System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0,$by,0,$by.Length);$img.UnlockBits($bd);$img.Dispose()

$names=@("k2-cream","k2-orange","k2-dark")
"knob face/skirt analysis (ridge energy per row):"
for($i=0;$i -lt 3;$i++){
  $rx=[int]($w/3*$i); $rw=[int]($w/3)
  # bbox
  $xmin=999999;$xmax=-1;$ymin=999999;$ymax=-1
  for($y=0;$y -lt $h;$y++){for($x=$rx;$x -lt ($rx+$rw);$x++){
    $ii=$y*$st+$x*4
    if($by[$ii+3] -gt 40){if($x -lt $xmin){$xmin=$x};if($x -gt $xmax){$xmax=$x};if($y -lt $ymin){$ymin=$y};if($y -gt $ymax){$ymax=$y}}}}
  $bw=$xmax-$xmin+1; $bh=$ymax-$ymin+1
  # ridge energy: horizontal high-frequency content, high on the knurled skirt
  $bestY=-1
  $prof=@()
  for($y=$ymin;$y -le $ymax;$y+=4){
    $s=0.0;$cnt=0
    for($x=$xmin+[int]($bw*0.2);$x -lt ($xmax-[int]($bw*0.2));$x+=2){
      $i1=$y*$st+$x*4; $i2=$y*$st+($x+3)*4
      if($by[$i1+3] -gt 200 -and $by[$i2+3] -gt 200){
        $l1=0.299*$by[$i1+2]+0.587*$by[$i1+1]+0.114*$by[$i1]
        $l2=0.299*$by[$i2+2]+0.587*$by[$i2+1]+0.114*$by[$i2]
        $s+=[Math]::Abs($l1-$l2);$cnt++}}
    if($cnt -gt 0){ $prof += ,@($y,($s/$cnt)) }
  }
  # skirt starts where ridge energy first exceeds 3x the face median, in the lower half
  $vals=$prof | ForEach-Object { $_[1] } | Sort-Object
  $med=$vals[[int]($vals.Count*0.35)]
  $face=$ymax
  foreach($pp in $prof){ if($pp[0] -gt ($ymin+$bh*0.45) -and $pp[1] -gt ($med*3.0+1.5)){ $face=$pp[0]; break } }
  "  {0,-10} bbox {1}x{2} at ({3},{4})  median ridge {5:N2}  face ends y={6}  skirt={7}px ({8:P0} of height)" -f `
    $names[$i],$bw,$bh,$xmin,$ymin,$med,$face,($ymax-$face),(($ymax-$face)/$bh)
}
