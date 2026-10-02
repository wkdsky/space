"""Draw the ship terminal's small trash target source image."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

size = 128
icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
g = ImageDraw.Draw(glow)
g.rounded_rectangle((31, 36, 97, 109), radius=11, outline=(255, 95, 79, 150), width=9)
glow = glow.filter(ImageFilter.GaussianBlur(11))
icon.alpha_composite(glow)
d = ImageDraw.Draw(icon)
d.rounded_rectangle((34, 39, 94, 105), radius=8, fill=(36, 56, 70, 245), outline=(255, 133, 108, 255), width=5)
d.rounded_rectangle((26, 27, 102, 42), radius=6, fill=(255, 133, 108, 255))
d.rounded_rectangle((48, 17, 80, 30), radius=5, outline=(255, 188, 139, 255), width=5)
for x in (51, 64, 77):
    d.line((x, 56, x, 89), fill=(255, 188, 139, 255), width=5)

target = Path(__file__).resolve().parents[1] / "SourceAssets" / "UI" / "T_ShipTrash.png"
target.parent.mkdir(parents=True, exist_ok=True)
icon.save(target)
print(target)
