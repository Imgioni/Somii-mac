Add-Type -AssemblyName System.Drawing
$ErrorActionPreference="Stop"
$REF="C:\Users\w0nde\Desktop\Vst\docs\reference\assets"; $OUT="C:\Users\w0nde\Desktop\Vst\design"

# octave2 content bbox is x=36 w=1575 h=876, and 1575/7 = 225.0 = the measured
# white-key pitch, so the render is already exactly one C..B period.
$img=[System.Drawing.Bitmap]::FromFile("$REF\octave2.png")
$outW=480; $outH=[int][Math]::Round(480.0*876/1575)
$dst=New-Object System.Drawing.Bitmap($outW,$outH,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[System.Drawing.Graphics]::FromImage($dst)
$g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($img,(New-Object System.Drawing.Rectangle 0,0,$outW,$outH),36,0,1575,876,[System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose();$img.Dispose()
$dst.Save("$OUT\octave.png",[System.Drawing.Imaging.ImageFormat]::Png);$dst.Dispose()
"octave.png -> {0}x{1} (period 1575x876, aspect {2:N3}), {3:N1} KB" -f $outW,$outH,(1575/876),((Get-Item "$OUT\octave.png").Length/1KB)

"=== contact sheet: new parts on panel grey ==="
$W=1220;$H=680
$sheet=New-Object System.Drawing.Bitmap($W,$H,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g=[System.Drawing.Graphics]::FromImage($sheet)
$g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$pt=[System.Drawing.Bitmap]::FromFile("$OUT\panel.png")
for($y=0;$y -lt $H;$y+=256){for($x=0;$x -lt $W;$x+=256){$g.DrawImage($pt,$x,$y,256,256)}}
$pt.Dispose()
$font=New-Object System.Drawing.Font("Segoe UI",10,[System.Drawing.FontStyle]::Bold)
$br=New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(35,37,42))

function Place($name,$x,$y,$w,$h,$label,$rot){
  $b=[System.Drawing.Bitmap]::FromFile((Join-Path $OUT $name))
  $st=$g.Save()
  if($rot -ne 0){
    $g.TranslateTransform($x+$w/2,$y+$h/2); $g.RotateTransform($rot); $g.TranslateTransform(-($w/2),-($h/2))
    $g.DrawImage($b,0,0,$w,$h)
  } else { $g.DrawImage($b,$x,$y,$w,$h) }
  $g.Restore($st)
  if($label){ $g.DrawString($label,$font,$br,$x,$y+$h+3) }
  $b.Dispose()
}

# knob rotation sweep - the real test of the indicator
$g.DrawString("knob with indicator - full sweep -150 to +150",$font,$br,30,12)
$angles=@(-150,-100,-50,0,50,100,150)
for($i=0;$i -lt 7;$i++){ Place "k2-cream.png" (34+$i*84) 36 62 62 ("" + $angles[$i]) $angles[$i] }
for($i=0;$i -lt 7;$i++){ Place "k2-orange.png" (34+$i*84) 126 62 62 $null $angles[$i] }
for($i=0;$i -lt 7;$i++){ Place "k2-dark.png"  (34+$i*84) 206 62 62 $null $angles[$i] }

# at true panel size
$g.DrawString("at panel size (44px / 34px / 28px)",$font,$br,660,12)
Place "k2-cream.png"  668 36 44 44 $null -120
Place "k2-orange.png" 724 36 44 44 $null 0
Place "k2-dark.png"   780 36 44 44 $null 120
Place "k2-cream.png"  668 96 34 34 $null -60
Place "k2-orange.png" 716 96 34 34 $null 40
Place "k2-dark.png"   764 96 34 34 $null 140
Place "k2-cream.png"  668 146 28 28 $null -150
Place "k2-orange.png" 708 146 28 28 $null 0
Place "k2-dark.png"   748 146 28 28 $null 150

$g.DrawString("3-position switch",$font,$br,860,12)
Place "sw-top.png" 866 36 26 58 "TOP" 0
Place "sw-mid.png" 926 36 26 58 "MID" 0
Place "sw-bot.png" 986 36 26 58 "BOT" 0
Place "sw-top.png" 866 130 18 40 $null 0
Place "sw-mid.png" 906 130 18 40 $null 0
Place "sw-bot.png" 946 130 18 40 $null 0

$g.DrawString("61-key bed: octave x5 + top C",$font,$br,30,300)
$ot=[System.Drawing.Bitmap]::FromFile("$OUT\octave.png")
$oct=205.0
for($i=0;$i -lt 6;$i++){ $g.DrawImage($ot,(30+$i*$oct),322,$oct,175) }
$ot.Dispose()
$g.FillRectangle((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(204,204,202))),(30+5*$oct+$oct/7),322,200,180)

$g.DrawString("old vs new octave proportion (same width)",$font,$br,30,516)
$ot=[System.Drawing.Bitmap]::FromFile("$OUT\octave.png")
$g.DrawImage($ot,30,538,300,167); $ot.Dispose()

$g.Dispose();$sheet.Save("$OUT\_contact2.png",[System.Drawing.Imaging.ImageFormat]::Png);$sheet.Dispose()
"contact sheet written"
