$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$resources=$PSScriptRoot
$png=Join-Path $resources 'PokeFinderPlus-256.png'
$source=[System.Drawing.Bitmap]::FromFile($png)
if($source.Width -ne 256 -or $source.Height -ne 256){throw 'Expected supplied 256x256 artwork'}
$sizes=@(16,24,32,48,64,128,256)
$frames=New-Object 'System.Collections.Generic.List[byte[]]'
foreach($size in $sizes){
 if($size -eq 256){$frames.Add([IO.File]::ReadAllBytes($png));continue}
 $bitmap=New-Object System.Drawing.Bitmap $size,$size,([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
 $g=[System.Drawing.Graphics]::FromImage($bitmap)
 $g.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
 $g.CompositingQuality=[System.Drawing.Drawing2D.CompositingQuality]::HighQuality
 $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
 $g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::HighQuality
 $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
 $attrs=New-Object System.Drawing.Imaging.ImageAttributes
 $attrs.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
 $g.DrawImage($source,(New-Object System.Drawing.Rectangle 0,0,$size,$size),0,0,256,256,[System.Drawing.GraphicsUnit]::Pixel,$attrs)
 $stream=New-Object IO.MemoryStream
 $bitmap.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
 $frames.Add($stream.ToArray())
 $attrs.Dispose();$g.Dispose();$bitmap.Dispose();$stream.Dispose()
}
$source.Dispose()
$outStream=[IO.File]::Create((Join-Path $resources 'pokefinder-plus.ico'))
$writer=New-Object IO.BinaryWriter $outStream
$writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
$offset=6+16*$sizes.Count
for($n=0;$n -lt $sizes.Count;$n++){
 $dimension=if($sizes[$n] -eq 256){0}else{$sizes[$n]}
 $writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([byte]0);$writer.Write([byte]0)
 $writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$frames[$n].Length);$writer.Write([uint32]$offset)
 $offset+=$frames[$n].Length
}
foreach($frame in $frames){$writer.Write($frame)}
$writer.Dispose();$outStream.Dispose()
Write-Output "Generated transparent ICO sizes: $($sizes -join ', ')"
