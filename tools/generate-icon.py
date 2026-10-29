"""Render the original two-key remapping icon at Windows icon sizes (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parent.parent / 'assets'
S = 4
im = Image.new('RGBA', (256*S, 256*S))
d = ImageDraw.Draw(im)
def box(bounds, radius, color, outline=None, width=1):
    d.rounded_rectangle(tuple(int(x*S) for x in bounds), radius*S, fill=color, outline=outline, width=width*S)
def polygon(points, color): d.polygon([(x*S,y*S) for x,y in points], fill=color)
box((4,4,252,252),54,'#111c32')
box((26,43,116,140),18,'#465dd9')
box((26,34,116,126),18,'#eef4ff')
box((140,136,230,230),18,'#253d87')
box((140,124,230,216),18,'#829aff')
# Key legends are geometric cutouts: source key and destination key.
polygon([(48,103),(65,57),(78,57),(96,103),(82,103),(79,92),(63,92),(60,103)],'#24355f')
polygon([(67,81),(75,81),(71,67)],'#eef4ff')
# Destination key: return arrow.
polygon([(203,148),(203,174),(177,174),(177,164),(160,180),(177,196),(177,186),(214,186),(214,148)],'#111c32')
# Two opposing arrows communicate remapping rather than text entry.
polygon([(130,53),(188,53),(188,40),(221,65),(188,90),(188,77),(130,77)],'#58e2cf')
polygon([(126,179),(70,179),(70,166),(37,191),(70,216),(70,203),(126,203)],'#58e2cf')
im.resize((256,256), Image.Resampling.LANCZOS).save(root/'MagicKey.png')
im.save(root/'MagicKey.ico', sizes=[(16,16),(20,20),(24,24),(32,32),(40,40),(48,48),(64,64),(128,128),(256,256)])
