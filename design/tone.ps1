Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$SRC="C:\Users\w0nde\Desktop\Vst\resources"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

# Re-cut the panel surface, then lift exposure and warm the white balance.
# Target a warm off-white around #E6E2D9 instead of the flat #CCCCCA the
# product shot tiles out at. Grain is preserved - only level and balance move.
$src=[System.Drawing.Bitmap]::FromFile("$SRC\panel_surface.png")
$q=128
$patch=New-Object System.Drawing.Bitmap($q,$q,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($patch)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($src,(New-Object System.Drawing.Rectangle 0,0,$q,$q),387,387,480,480,[System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose();$src.Dispose()

$gr=230.0/204; $gg=226.0/204; $gb=217.0/202
$warm=New-Object System.Drawing.Bitmap($q,$q,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
for($y=0;$y -lt $q;$y++){for($x=0;$x -lt $q;$x++){
  $p=$patch.GetPixel($x,$y)
  $r=[int][Math]::Round($p.R*$gr); $g2=[int][Math]::Round($p.G*$gg); $b=[int][Math]::Round($p.B*$gb)
  if($r -gt 255){$r=255}; if($g2 -gt 255){$g2=255}; if($b -gt 255){$b=255}
  $warm.SetPixel($x,$y,[System.Drawing.Color]::FromArgb($r,$g2,$b))}}
$patch.Dispose()

# mirror 2x2 for a seamless tile
$tile=New-Object System.Drawing.Bitmap(($q*2),($q*2),[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($tile)
$g.DrawImage($warm,0,0,$q,$q)
$a=$warm.Clone();$a.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipX);$g.DrawImage($a,$q,0,$q,$q)
$b2=$warm.Clone();$b2.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipY);$g.DrawImage($b2,0,$q,$q,$q)
$c=$warm.Clone();$c.RotateFlip([System.Drawing.RotateFlipType]::RotateNoneFlipXY);$g.DrawImage($c,$q,$q,$q,$q)
$g.Dispose();$warm.Dispose();$a.Dispose();$b2.Dispose();$c.Dispose()
$tile.Save("$OUT\panel.png",[System.Drawing.Imaging.ImageFormat]::Png)

# report the new base colour
$sr=0.0;$sg=0.0;$sb=0.0;$n=0
for($y=0;$y -lt 256;$y+=4){for($x=0;$x -lt 256;$x+=4){$p=$tile.GetPixel($x,$y);$sr+=$p.R;$sg+=$p.G;$sb+=$p.B;$n++}}
"panel.png -> 256x256 warm, base #{0:X2}{1:X2}{2:X2}, {3:N1} KB" -f [int]($sr/$n),[int]($sg/$n),[int]($sb/$n),((Get-Item "$OUT\panel.png").Length/1KB)
$tile.Dispose()

# lighten the shading pass so it models form without dulling the surface
$sh=[System.Drawing.Bitmap]::FromFile("$SRC\shading.png")
$dst=New-Object System.Drawing.Bitmap(400,225,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($sh,0,0,400,225); $g.Dispose(); $sh.Dispose()
# pull it toward white so a multiply blend only just tints
$soft=New-Object System.Drawing.Bitmap(400,225,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
for($y=0;$y -lt 225;$y++){for($x=0;$x -lt 400;$x++){
  $p=$dst.GetPixel($x,$y)
  $r=[int](255-(255-$p.R)*0.45); $g3=[int](255-(255-$p.G)*0.45); $b=[int](255-(255-$p.B)*0.42)
  $soft.SetPixel($x,$y,[System.Drawing.Color]::FromArgb($r,$g3,$b))}}
$dst.Dispose()
$enc=[System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders()|Where-Object{$_.MimeType -eq 'image/jpeg'}
$pars=New-Object System.Drawing.Imaging.EncoderParameters(1)
$pars.Param[0]=New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality,86)
$soft.Save("$OUT\shading.jpg",$enc,$pars); $soft.Dispose()
"shading.jpg -> 400x225 softened, {0:N1} KB" -f ((Get-Item "$OUT\shading.jpg").Length/1KB)
