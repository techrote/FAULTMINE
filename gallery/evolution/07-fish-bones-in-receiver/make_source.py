#!/usr/bin/env python3
"""FISH BONES IN THE RECEIVER: self-authored 384x256 RGB BMP, stdlib only.
No external photographs, fonts, clip art, randomness or network.
"""
import argparse
import hashlib
import pathlib
import struct

W,H=384,256
DEST=pathlib.Path(__file__).with_name('source.bmp')
C={'abyss':(7,17,26),'grain':(14,32,42),'grid':(26,58,65),
   'halo':(32,73,78),'body':(28,93,93),'inner':(49,126,116),
   'bone':(227,218,171),'glow':(139,237,204),'rust':(227,101,73),
   'spark':(255,192,109),'ghost':(75,110,113)}
p=bytearray(bytes(C['abyss'])*(W*H))
def dot(x,y,c):
    if 0<=x<W and 0<=y<H:
        i=(y*W+x)*3;p[i:i+3]=bytes(c)
def rect(x0,y0,x1,y1,c):
    for y in range(max(0,y0),min(H,y1)):
        for x in range(max(0,x0),min(W,x1)):dot(x,y,c)
def line(x0,y0,x1,y1,c,w=1):
    dx=abs(x1-x0);dy=abs(y1-y0);sx=1 if x0<x1 else -1;sy=1 if y0<y1 else -1;err=dx-dy
    while True:
        for oy in range(-(w//2),(w+1)//2):
            for ox in range(-(w//2),(w+1)//2):dot(x0+ox,y0+oy,c)
        if x0==x1 and y0==y1:break
        e2=2*err
        if e2>-dy:err-=dy;x0+=sx
        if e2<dx:err+=dx;y0+=sy

def ellipse(cx,cy,rx,ry,c,fill=False):
    if fill:
        for y in range(cy-ry,cy+ry+1):
            for x in range(cx-rx,cx+rx+1):
                if ((x-cx)*(x-cx)*ry*ry+(y-cy)*(y-cy)*rx*rx)<=rx*rx*ry*ry:dot(x,y,c)
    else:
        # A deliberately thin deterministic outline, not a WIC antialiasing dependency.
        for y in range(cy-ry,cy+ry+1):
            for x in range(cx-rx,cx+rx+1):
                v=(x-cx)*(x-cx)*ry*ry+(y-cy)*(y-cy)*rx*rx
                if abs(v-rx*rx*ry*ry)<max(1,rx*ry*2):dot(x,y,c)

# A rectilinear survey apparatus, kept subordinate to a biological silhouette.
for x in range(20,364,16):
    for y in range(16,240,8):
        if (x+3*y)%7<3:dot(x,y,C['grain'])
for y in (31,216):
    for x in range(18,364,4):
        if x%28<20:dot(x,y,C['grid'])
for x in (22,361):
    line(x,25,x,232,C['grid'])
    for y in range(28,231,12):line(x-2,y,x+2,y,C['ghost'])

# Echolocation arcs, behind the fish. Intentionally incomplete circles.
for cx,cy,rad in ((293,104,56),(293,104,80),(290,105,108)):
    for x in range(max(0,cx-rad),min(W,cx+rad+1)):
        dx=x-cx
        rr=rad*rad-dx*dx
        if rr<0:continue
        yoff=int(rr**0.5)
        for y in (cy-yoff,cy+yoff):
            if 0<=y<H and (x//9)%5!=1 and (x+2*y)%13<9:dot(x,y,C['halo'])

# Piecewise continuously eased centreline: exact integer arithmetic, no RNG.
knots=[(40,189),(84,180),(126,144),(165,119),(205,125),(246,100),(291,73),(327,72)]
points={}
for (x0,y0),(x1,y1) in zip(knots,knots[1:]):
    den=x1-x0
    for x in range(x0,x1+1):
        t=(x-x0)*256//den
        smooth=t*t*(768-2*t)//65536
        points[x]=(y0*(256-smooth)+y1*smooth+128)//256

# Shadow and outer body, scaled down at tail and toward snout.
for x in range(39,327):
    cy=points.get(x,72)
    q=x-39
    radius=min(22,5+q//8,5+(327-x)//4)
    for dy in range(-radius-3,radius+4):
        if abs(dy)<=radius+2:dot(x,cy+dy,C['halo'])
    for dy in range(-radius,radius+1):
        dot(x,cy+dy,C['body'] if abs(dy)>max(2,radius-5) else C['inner'])

# Repeated directional scales, visibly related but never just cloned stripes.
for x in range(57,317,11):
    cy=points[x]
    r=min(21,5+(x-39)//8,5+(327-x)//4)
    for d in range(-r+2,r-1,5):
        if (x//11+d)%3:line(x-4,cy+d,x+1,cy+d+2,C['body'])

# Spine and alternating transverse ribs. Skeleton survives moderate misaddressing.
for x in range(40,326):
    cy=points[x];dot(x,cy-1,C['bone']);dot(x,cy,C['bone'])
for x in range(70,313,12):
    cy=points[x];r=min(20,5+(x-39)//8,5+(327-x)//4)
    normal=3 if (x//12)%2 else -3
    line(x,cy,x-5,cy-r+3,C['bone'],2)
    line(x,cy,x+normal,cy+r-3,C['glow'],2)
    dot(x,cy,C['spark'])

# A blunt masked head, jaws, two mismatched eyes, and a small forked tail.
ellipse(331,71,19,13,C['inner'],True)
line(335,74,355,80,C['bone'],3)
line(337,78,353,85,C['rust'],2)
ellipse(337,66,4,4,C['rust'],True)
dot(338,65,C['spark'])
line(41,189,20,174,C['glow'],2)
line(41,190,18,206,C['bone'],2)
line(24,174,14,168,C['ghost'])
line(24,205,14,211,C['ghost'])

# Deliberate missing measurements: exterior dots, three contact scars.
for cx,cy in ((42,65),(103,46),(352,188)):
    rect(cx-8,cy-1,cx+9,cy+1,C['grid'])
    rect(cx-1,cy-8,cx+1,cy+9,C['grid'])
    rect(cx-2,cy-2,cx+3,cy+3,C['rust'])
for cx,cy,r in ((76,87,4),(113,73,2),(143,53,3),(190,199,3),(237,173,5)):
    ellipse(cx,cy,r,r,C['ghost'])
    dot(cx-r,cy,C['glow'])
# Solid data plates at opposite edges; one absent inset registers as a void.
rect(27,230,143,233,C['ghost'])
for i in range(9):rect(28+i*13,235,30+i*13,240,C['rust'] if i==5 else C['grid'])
rect(272,21,355,24,C['ghost'])
for i in range(10):
    x=273+i*8
    rect(x,17,x+2,20,C['spark'] if i in (2,7) else C['grid'])

stride=(W*3+3)&~3
out=bytearray(b'BM')+struct.pack('<IHHI',54+stride*H,0,0,54)
out+=struct.pack('<IiiHHIIiiII',40,W,H,1,24,0,stride*H,2835,2835,0,0)
for y in range(H-1,-1,-1):
    for x in range(W):
        r,g,b=p[(y*W+x)*3:(y*W+x)*3+3]
        out+=bytes((b,g,r))
    out+=b'\0'*(stride-3*W)
bmp_sha=hashlib.sha256(out).hexdigest()
rgba=bytearray()
for i in range(0,len(p),3):rgba+=p[i:i+3]+b'\xff'
source_id=hashlib.sha256(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',W,H)+rgba).hexdigest()
def main():
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--write',action='store_true')
    mode.add_argument('--verify',action='store_true')
    args=parser.parse_args()
    if args.write:
        if DEST.exists() and DEST.read_bytes()!=out:
            parser.error('refusing to replace non-identical pre-existing source.bmp')
        if not DEST.exists():DEST.write_bytes(out)
    if not DEST.is_file() or DEST.read_bytes()!=out:
        parser.error('source.bmp missing or unequal to complete deterministic constructor')
    print('bmp_sha256='+bmp_sha)
    print('source_identity='+source_id)
    print('dimensions=384x256')
if __name__=='__main__':main()
