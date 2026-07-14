## Star map for the northern celestial hemisphere, magnitude 1–6
- approximately ~4,000 stars
- azimuthal equidistant, North Celestial Pole centered projection
- Symbols:
  * Mag ≤ 2: 4 px square
  * Mag 3–4: 3 px square
  * Mag 5: 2 px square
  * Mag 6: single-pixel dot

You can use https://splitter.imageonline.co/ to split images if you do not have [imagemagick](https://imagemagick.org) installed where you could most likely do just this:

```
convert starmap_input_file.png \
  -resize 640x640! \
  -threshold 50% \
  -crop 128x64 \
  +repage \
  -set filename:tile "%02d" \
  +adjoin \
  "%[filename:tile].png"
```

The following PowerShell-script converts a `PNG`-file to `BMP`-file:
```Add-Type -AssemblyName System.Drawing; Get-ChildItem *.png | Where-Object { $_.BaseName -match '^\d+$' } | Sort-Object { [int]$_.BaseName } | ForEach-Object { $p=$_.FullName; $b=[System.Drawing.Bitmap]::FromFile($p); $b.Save([System.IO.Path]::ChangeExtension($p,".bmp"),[System.Drawing.Imaging.ImageFormat]::Bmp); $b.Dispose() }
```


