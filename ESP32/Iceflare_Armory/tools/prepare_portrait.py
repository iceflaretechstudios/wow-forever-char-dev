"""Convert an existing Iceflare character portrait image to an ESP32 JPEG.
Usage: python tools/prepare_portrait.py path/to/portrait.png
Requires: pip install Pillow
"""
from pathlib import Path
import sys
from PIL import Image
if len(sys.argv) != 2:
    raise SystemExit('Usage: python tools/prepare_portrait.py <portrait image>')
source = Path(sys.argv[1]); out = Path(__file__).resolve().parent.parent / 'data' / 'portrait.jpg'
with Image.open(source) as im:
    im = im.convert('RGBA')
    canvas = Image.new('RGBA', im.size, (13, 24, 42, 255))
    canvas.alpha_composite(im)
    im = canvas.convert('RGB')
    im.thumbnail((220, 165))
    im.save(out, 'JPEG', quality=78, optimize=True)
print('Saved', out)
