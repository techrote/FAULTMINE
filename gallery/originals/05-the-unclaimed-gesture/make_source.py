#!/usr/bin/env python3
"""Author's offline deterministic construction of The Unclaimed Gesture source.
Creates the exact 320x224 indexed PNG using Pillow, for original artwork development only.
No existing artist images/fonts or other remote resources are used.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont
W,H=320,224
COLORS=[(7,14,23),(17,31,41),(28,51,61),(42,74,82),(71,123,130),(119,205,197),(220,222,186),(240,129,81),(141,75,79),(58,105,117),(180,202,192),(16,44,55),(240,194,115),(13,25,34),(106,155,162),(228,240,218)]
im=Image.new('P',(W,H),0)
pal=sum(([r,g,b] for r,g,b in COLORS),[])+[0,0,0]*(256-len(COLORS)); im.putpalette(pal)
d=ImageDraw.Draw(im)
# Background is an inactive specimen scanner, not a photographic setting.
for x in range(12,309,12): d.line((x,13,x,206),fill=1 if (x//12)%2==0 else 13,width=1)
for y in range(12,213,12): d.line((9,y,311,y),fill=13,width=1)
d.rectangle((13,12,307,210),outline=3,width=1)
d.line((13,24,307,24),fill=2,width=1);d.line((13,199,307,199),fill=2,width=1)
for y in range(32,195,8):
    d.line((13,y,19,y),fill=4 if y%24 else 7,width=1)
    d.line((301,y,307,y),fill=4 if y%24 else 7,width=1)
# The right half has broad empty space with a lightly registered lost image.
for y in range(43,166,22):
    d.line((257,y,291,y),fill=2,width=1)
    d.line((291,y-3,291,y+3),fill=4,width=1)
# Hand is an authored outline/mask, five joints plus massive curved palm.
mask=Image.new('1',(W,H),0);m=ImageDraw.Draw(mask)
# broad palm and cuff
m.polygon([(108,122),(129,101),(147,105),(165,119),(179,139),(185,163),(178,185),(162,194),(128,189),(102,171),(95,143)],fill=1)
m.polygon([(115,175),(159,185),(154,213),(107,207),(98,184)],fill=1)
# thumb swings left, separated from upright fingers
m.polygon([(106,142),(90,139),(68,125),(54,113),(50,103),(54,96),(61,95),(69,101),(93,117),(119,128)],fill=1)
# index / middle / ring / little fingertips: intentionally uneven, but anatomically ordered
m.polygon([(111,126),(104,104),(99,75),(92,52),(89,37),(93,31),(100,29),(107,35),(115,59),(128,114)],fill=1)
m.polygon([(127,120),(120,87),(119,48),(122,21),(128,14),(136,16),(140,25),(141,65),(149,117)],fill=1)
m.polygon([(146,117),(149,84),(159,52),(165,35),(172,32),(179,36),(180,45),(171,71),(167,120)],fill=1)
m.polygon([(161,128),(181,102),(198,82),(207,79),(214,84),(214,91),(193,124),(175,143)],fill=1)
# offset cyan incomplete latent register: hand's earlier position, not a second whole hand
bbox=mask.getbbox();latent=mask.copy();ld=ImageDraw.Draw(im)
from PIL import ImageChops
edge=ImageChops.subtract(mask.filter(ImageFilter.MaxFilter(3)),mask.filter(ImageFilter.MinFilter(3)))
# Subtle dislocated ghost behind main, in distinct offset registration channels.
ghost=Image.new('1',(W,H),0);ghost.paste(edge,(10,6));im.paste(4,(0,0,W,H),ghost)
# Distinct body fill & perimeter
im.paste(11,(0,0,W,H),mask)
ring=mask.filter(ImageFilter.MaxFilter(3));outline=ImageChops.subtract(ring,mask)
im.paste(6,(0,0,W,H),outline)
d=ImageDraw.Draw(im)
# independent etched tendons: structural vocabulary, no stock biological imagery
d.line([(129,160),(130,122),(125,99),(120,74),(120,48)],fill=4,width=1)
d.line([(139,156),(146,129),(151,98),(163,56)],fill=5,width=1)
d.line([(150,163),(165,148),(180,123),(198,96)],fill=3,width=1)
d.line([(119,151),(103,133),(80,119),(61,106)],fill=5,width=1)
for y,x1,x2 in [(147,113,174),(156,106,176),(165,110,178),(175,117,170),(184,129,162)]:
    d.line((x1,y,x2,y+1),fill=3,width=1)
# crossed register along cuff; one single absent scan-row is precomposed, fault operators develop it.
d.rectangle((104,185,162,195),fill=0)
for x in range(108,160,5):d.line((x,188,x+3,188),fill=7 if x%3 else 5,width=1)
d.line((104,187,159,187),fill=10,width=1)
# Four imperfect touch point labels, sixth index deliberately empty
for x,y,t in [(96,26,'01'),(171,25,'03'),(205,73,'04')]:
    d.line((x,y-6,x,y-1),fill=7,width=1)
    d.text((x-4,y-15),t,fill=6,font=ImageFont.load_default())
d.text((143,11),'02',fill=6,font=ImageFont.load_default())
d.line((139,20,143,19),fill=7,width=1)
# abandoned fifth record remains on right as an empty registration aperture
for x in (249,273):d.line((x,88,x,115),fill=8,width=1)
d.line((249,88,273,88),fill=8,width=1)
d.line((249,115,273,115),fill=8,width=1)
d.line((253,101,269,101),fill=3,width=1)
d.text((246,119),'05?',fill=7,font=ImageFont.load_default())
d.line((216,103,244,103),fill=8,width=1)
# Independent angular scission scars along the palm that the fault stack may displace
for a,b in [((104,147),(120,150)),((136,106),(148,110)),((169,143),(181,140))]:
    d.line((*a,*b),fill=7,width=2)
# Finite registration aids on far edges; angular measurement legend.
d.text((23,15),'FINGER / 00',fill=4,font=ImageFont.load_default())
d.text((228,15),'NO RETURN',fill=7,font=ImageFont.load_default())
d.text((22,201),'GESTURE LOST',fill=6,font=ImageFont.load_default())
d.text((240,201),'R-005',fill=4,font=ImageFont.load_default())
# Signal seam across the supposedly blank zone
for x in range(228,294,7):d.point((x,179+(x*17%7)),fill=3)
# output exact indexed source
out=Path(__file__).with_name('source.png');im.save(out,format='PNG',optimize=True,compress_level=9)
print('wrote',out,out.stat().st_size)
