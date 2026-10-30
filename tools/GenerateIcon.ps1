Add-Type -AssemblyName System.Drawing
$iconPath=Join-Path $PSScriptRoot '..\Assets\App.ico'
$frames=@()
foreach($size in @(16,24,32,48,64,128,256)) {
    $canvas=New-Object System.Drawing.Bitmap 1024,1024
    $g=[System.Drawing.Graphics]::FromImage($canvas)
    $g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    # Transparent canvas; enlarge the existing artwork to fit without a tile.
    $transform=New-Object System.Drawing.Drawing2D.Matrix 1.3,0,0,1.3,-188.7,-158.7
    $g.Transform=$transform
    # Bold cursor silhouette, legible in the taskbar and notification area.
    $points=[System.Drawing.PointF[]]@([System.Drawing.PointF]::new(300,200),[System.Drawing.PointF]::new(300,750),[System.Drawing.PointF]::new(430,628),[System.Drawing.PointF]::new(542,839),[System.Drawing.PointF]::new(664,775),[System.Drawing.PointF]::new(552,565),[System.Drawing.PointF]::new(744,565))
    $outline=New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,13,33,74)),32
    $outline.LineJoin=[System.Drawing.Drawing2D.LineJoin]::Round
    $g.FillPolygon([System.Drawing.Brushes]::White,$points);$g.DrawPolygon($outline,$points)
    # Larger plus sign with the same dark outline as the arrow.
    $accentOutline=New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,13,33,74)),88
    $accent=New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255,139,224,255)),56
    $accentOutline.StartCap=$accentOutline.EndCap=[System.Drawing.Drawing2D.LineCap]::Round
    $accent.StartCap=$accent.EndCap=[System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($accentOutline,630,280,806,280);$g.DrawLine($accentOutline,718,192,718,368)
    $g.DrawLine($accent,630,280,806,280);$g.DrawLine($accent,718,192,718,368)
    $resized=New-Object System.Drawing.Bitmap $size,$size
    $r=[System.Drawing.Graphics]::FromImage($resized);$r.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $r.DrawImage($canvas,0,0,$size,$size)
    $stream=New-Object System.IO.MemoryStream
    $resized.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    $frames+=,@{Size=$size;Bytes=$stream.ToArray()}
    if($size -eq 256){$resized.Save((Join-Path $PSScriptRoot '..\Assets\App.png'),[System.Drawing.Imaging.ImageFormat]::Png)}
    $stream.Dispose();$r.Dispose();$resized.Dispose();$accent.Dispose();$accentOutline.Dispose();$outline.Dispose();$transform.Dispose();$g.Dispose();$canvas.Dispose()
}
$file=[System.IO.File]::Create($iconPath);$writer=New-Object System.IO.BinaryWriter $file
$writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$frames.Count)
$offset=6+16*$frames.Count
foreach($frame in $frames) {
    $dimension=if($frame.Size -eq 256){0}else{$frame.Size}
    $writer.Write([byte]$dimension);$writer.Write([byte]$dimension);$writer.Write([byte]0);$writer.Write([byte]0)
    $writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$frame.Bytes.Length);$writer.Write([uint32]$offset)
    $offset+=$frame.Bytes.Length
}
foreach($frame in $frames){$writer.Write([byte[]]$frame.Bytes)}
$writer.Dispose();$file.Dispose()
