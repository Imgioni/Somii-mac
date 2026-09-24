$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$source = Join-Path $PSScriptRoot '..\docs\reference\assets\New assets'
$target = Join-Path $PSScriptRoot '..\ui'
$jobs = @(
  'button-light|Light pushbutton.png|307|310|640|634', 'button-light-down|Light pushbutton pushed.png|307|310|640|634',
  'button-dark|Dark pushbutton.png|307|310|640|634', 'button-dark-down|Dark pushbutton pushed.png|307|310|640|634',
  'fader-grey|Fader cap white.png|258|410|739|526', 'fader-orange|Fader cap orange.png|258|410|739|526', 'fader-dark|Fader cap black.png|258|410|739|526',
  'rail|Fader rail.png|432|115|156|1310', 'led-on|Indicator LED on.png|414|402|426|420', 'led-off|Indicator LED off.png|414|402|426|420',
  'ribbon-strip|Ribbon strip.png|24|265|2124|188', 'ribbon-left|Ribbon left end.png|258|348|891|420', 'ribbon-right|Ribbon right end.png|381|222|1047|477',
  'octave-left|Horizontal octave lever 3.png|225|336|999|414', 'octave-center|Horizontal octave lever 1.png|225|336|999|414', 'octave-right|Horizontal octave lever 2.png|225|336|999|414',
  'bender-center|Bender lever 1.png|116|80|550|250', 'bender-center-push|Bender lever 1.png|116|374|550|250',
  'bender-left|Bender lever 1.png|820|80|550|250', 'bender-left-push|Bender lever 1.png|820|374|550|250',
  'bender-right|Bender lever 1.png|1524|80|550|250', 'bender-right-push|Bender lever 1.png|1524|374|550|250',
  'knob-cream|Standard rotary knob.png|174|76|510|565', 'knob-cream-face|Standard rotary knob.png|220|86|410|374',
  'knob-orange|Standard rotary knob.png|830|76|510|565', 'knob-orange-face|Standard rotary knob.png|875|86|410|374',
  'knob-dark|Standard rotary knob.png|1494|76|510|565', 'knob-dark-face|Standard rotary knob.png|1538|86|410|374',
  'small-cream|Small rotary knob 1.png|502|164|390|466', 'small-cream-face|Small rotary knob 1.png|552|174|288|260',
  'small-orange|Small rotary knob 1.png|1113|164|390|466', 'small-orange-face|Small rotary knob 1.png|1162|174|288|260'
)
foreach ($job in $jobs) {
  $p = $job.Split('|'); $name = $p[0]; $file = $p[1]; $x = [int]$p[2]; $y = [int]$p[3]; $w = [int]$p[4]; $h = [int]$p[5]
  $input = [System.Drawing.Bitmap]::FromFile((Join-Path $source $file))
  try {
    $output = [System.Drawing.Bitmap]::new($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
      $g = [System.Drawing.Graphics]::FromImage($output)
      try { $g.DrawImage($input, [System.Drawing.Rectangle]::new(0, 0, $w, $h), $x, $y, $w, $h, [System.Drawing.GraphicsUnit]::Pixel) }
      finally { $g.Dispose() }
      $output.Save((Join-Path $target "new-$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $output.Dispose() }
  } finally { $input.Dispose() }
}
# Three-position toggle: the supplied button sits inside the supplied base.
# Base crop 476,84 300x1090 (slot opening measured at x 543-711, y 148-1112).
# Button (opaque 410,240 432x774) is scaled to 150 px wide and parked 10 px inside the
# top end, the centre, or 10 px inside the bottom end of the opening.
$base = [System.Drawing.Bitmap]::FromFile((Join-Path $source 'Vertical toggle base.png'))
$button = [System.Drawing.Bitmap]::FromFile((Join-Path $source 'Vertical toggle button.png'))
try {
  $bw = 150; $bh = [int][Math]::Round(774 * $bw / 432)
  $bx = 627 - 476 - [int]($bw / 2)
  $top = 148 + 10 - 84; $bot = 1112 - 10 - 84 - $bh; $mid = [int](($top + $bot) / 2)
  foreach ($d in @(@('top', $top), @('mid', $mid), @('bot', $bot))) {
    $out = [System.Drawing.Bitmap]::new(300, 1090, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
      $g = [System.Drawing.Graphics]::FromImage($out)
      try {
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.DrawImage($base, [System.Drawing.Rectangle]::new(0, 0, 300, 1090), 476, 84, 300, 1090, [System.Drawing.GraphicsUnit]::Pixel)
        # the button is lifted to a mid grey so it stands out from the black slot
        $attr = [System.Drawing.Imaging.ImageAttributes]::new()
        $cm = [System.Drawing.Imaging.ColorMatrix]::new()
        $cm.Matrix00 = 1.3; $cm.Matrix11 = 1.3; $cm.Matrix22 = 1.3
        $cm.Matrix40 = 0.27; $cm.Matrix41 = 0.27; $cm.Matrix42 = 0.28
        $attr.SetColorMatrix($cm)
        $g.DrawImage($button, [System.Drawing.Rectangle]::new($bx, [int]$d[1], $bw, $bh), 410, 240, 432, 774, [System.Drawing.GraphicsUnit]::Pixel, $attr)
        $attr.Dispose()
      } finally { $g.Dispose() }
      $out.Save((Join-Path $target "new-switch-$($d[0]).png"), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $out.Dispose() }
  }
} finally { $base.Dispose(); $button.Dispose() }
Write-Output "Cropped $($jobs.Count) UI sprites and composed 3 toggle detents."

exit 0
