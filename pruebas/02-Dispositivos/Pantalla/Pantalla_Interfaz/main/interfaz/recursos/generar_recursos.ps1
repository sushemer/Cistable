param(
    [string]$ModeloA = 'C:\Users\HP\Downloads\Camion 1 (Modelo A)',
    [string]$ModeloB = 'C:\Users\HP\Downloads\Camion 2 (Modelo B)'
)
$ErrorActionPreference = 'Stop'
# Conversion offline: no se necesitan PNG ni System.Drawing en el ESP32.
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System.Drawing;
public static class LimitesImagen {
    public static Rectangle Obtener(Bitmap b) {
        int x0=b.Width, y0=b.Height, x1=-1, y1=-1;
        for(int y=0;y<b.Height;y++) for(int x=0;x<b.Width;x++) {
            if(b.GetPixel(x,y).A <= 8) continue;
            x0=System.Math.Min(x0,x); y0=System.Math.Min(y0,y);
            x1=System.Math.Max(x1,x); y1=System.Math.Max(y1,y);
        }
        if(x1<0) throw new System.ArgumentException("Imagen vacia");
        return Rectangle.FromLTRB(x0,y0,x1+1,y1+1);
    }
}
'@
$utf8 = New-Object System.Text.UTF8Encoding($false)
function Escribir($nombre, $texto) {
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot $nombre), $texto, $utf8)
}
function DatosC($bytes) {
    $lineas = for ($i=0; $i -lt $bytes.Count; $i+=16) {
        '    ' + (($bytes[$i..([Math]::Min($i+15,$bytes.Count-1))] | ForEach-Object { '0x{0:X2}' -f $_ }) -join ', ') + ','
    }
    $lineas -join "`n"
}
$entradas = @($ModeloA, $ModeloB)
for ($i=0; $i -lt 2; $i++) {
    $src = [Drawing.Bitmap]::FromFile($entradas[$i])
    $rect = [LimitesImagen]::Obtener($src)
    $dst = New-Object Drawing.Bitmap(160,90)
    $g = [Drawing.Graphics]::FromImage($dst)
    $g.Clear([Drawing.Color]::Transparent)
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $escala = [Math]::Min(156.0/$rect.Width,86.0/$rect.Height)
    $w = [int][Math]::Round($rect.Width*$escala)
    $h = [int][Math]::Round($rect.Height*$escala)
    $destino = New-Object Drawing.Rectangle(([int]((160-$w)/2)),([int]((90-$h)/2)),$w,$h)
    $g.DrawImage($src,$destino,$rect,[Drawing.GraphicsUnit]::Pixel)
    $nombre = if($i -eq 0){'camion_pequeno'}else{'camion_grande'}
    $dst.Save((Join-Path $PSScriptRoot "$nombre.png"),[Drawing.Imaging.ImageFormat]::Png)
    $bytes = New-Object 'System.Collections.Generic.List[byte]'
    for($y=0;$y -lt 90;$y++){ for($x=0;$x -lt 160;$x++){
        $c=$dst.GetPixel($x,$y)
        $bytes.Add($c.B); $bytes.Add($c.G); $bytes.Add($c.R); $bytes.Add($c.A)
    }}
    $datos = DatosC $bytes
    Escribir "$nombre.c" @"
/* Generado con generar_recursos.ps1. ARGB8888 little-endian (B,G,R,A). */
#include "camiones.h"
static const uint8_t pixeles[] = {
$datos
};
const lv_image_dsc_t $nombre = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
               .w = 160, .h = 90, .stride = 640},
    .data_size = sizeof(pixeles),
    .data = pixeles,
};
"@
    $g.Dispose(); $dst.Dispose(); $src.Dispose()
}
# Glifo local U+00F1, Montserrat Medium 24 px. No cambia la configuracion LVGL.
$ttf = Join-Path $PSScriptRoot '../../../managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf'
$coleccion = New-Object Drawing.Text.PrivateFontCollection
$coleccion.AddFontFile([IO.Path]::GetFullPath($ttf))
$familia = $coleccion.Families[0]
$path = New-Object Drawing.Drawing2D.GraphicsPath
$formato = [Drawing.StringFormat]::GenericTypographic.Clone()
$path.AddString([string][char]0xF1,$familia,0,24,[Drawing.PointF]::new(0,0),$formato)
$bounds = $path.GetBounds()
$x0=[int][Math]::Floor($bounds.Left); $y0=[int][Math]::Floor($bounds.Top)
$w=[int][Math]::Ceiling($bounds.Right)-$x0; $h=[int][Math]::Ceiling($bounds.Bottom)-$y0
$m=New-Object Drawing.Drawing2D.Matrix
$m.Translate(-$x0,-$y0); $path.Transform($m)
$bmp=New-Object Drawing.Bitmap($w,$h)
$g=[Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.FillPath([Drawing.Brushes]::White,$path)
$font=New-Object Drawing.Font($familia,24,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
$avance=[int][Math]::Round($g.MeasureString([string][char]0xF1,$font,100,$formato).Width*16)
$baseline=24.0*$familia.GetCellAscent(0)/$familia.GetEmHeight(0)
$offsetY=[int][Math]::Round($baseline)-($y0+$h)
$bytes=New-Object 'System.Collections.Generic.List[byte]'
for($y=0;$y -lt $h;$y++){for($x=0;$x -lt $w;$x++){$bytes.Add($bmp.GetPixel($x,$y).A)}}
$datos=DatosC $bytes
Escribir 'fuente_enie.c' @"
/* U+00F1 de Montserrat Medium 24 px; generado con generar_recursos.ps1. */
#include "camiones.h"
static const uint8_t bitmap[] = {
$datos
};
static const lv_font_fmt_txt_glyph_dsc_t glifos[] = {
    {0},
    {.bitmap_index=0, .adv_w=$avance, .box_w=$w, .box_h=$h, .ofs_x=$x0, .ofs_y=$offsetY},
};
static const lv_font_fmt_txt_cmap_t mapa[] = {
    {.range_start=0xF1, .range_length=1, .glyph_id_start=1,
     .type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},
};
static const lv_font_fmt_txt_dsc_t descriptor = {
    .glyph_bitmap=bitmap, .glyph_dsc=glifos, .cmaps=mapa, .cmap_num=1, .bpp=8,
};
const lv_font_t fuente_enie_24 = {
    .get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,
    .line_height=27, .base_line=5, .dsc=&descriptor,
};
"@
$font.Dispose(); $g.Dispose(); $bmp.Dispose(); $m.Dispose(); $path.Dispose(); $formato.Dispose(); $coleccion.Dispose()
