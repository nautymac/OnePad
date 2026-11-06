# Generates src\onepad.ico (simple 32x32 + 16x16 icon: dark rounded square with two pad dots) using System.Drawing.
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Add-Type -AssemblyName System.Drawing
$sizes = 16, 32, 48
$images = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([System.Drawing.Color]::Transparent)
    $bg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 36, 41, 56))
    $g.FillEllipse($bg, 0, 0, $s - 1, $s - 1)
    $dot = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 90, 200, 250))
    $r = [int]($s * 0.28); $y = [int](($s - $r) / 2)
    $g.FillEllipse($dot, [int]($s * 0.14), $y, $r, $r)
    $g.FillEllipse($dot, [int]($s * 0.58), $y, $r, $r)
    $g.Dispose()
    $images += $bmp
}
# write ICO manually (PNG-compressed entries)
$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter $ms
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$images.Count)
$pngs = @()
foreach ($img in $images) { $p = New-Object System.IO.MemoryStream; $img.Save($p, [System.Drawing.Imaging.ImageFormat]::Png); $pngs += ,$p.ToArray() }
$offset = 6 + 16 * $images.Count
for ($i = 0; $i -lt $images.Count; $i++) {
    $s = $images[$i].Width
    $bw.Write([byte]($(if ($s -ge 256) { 0 } else { $s }))); $bw.Write([byte]($(if ($s -ge 256) { 0 } else { $s })))
    $bw.Write([byte]0); $bw.Write([byte]0); $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$pngs[$i].Length); $bw.Write([uint32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Flush()
[System.IO.File]::WriteAllBytes("$root\src\onepad.ico", $ms.ToArray())
Write-Host "icon written: $root\src\onepad.ico"
