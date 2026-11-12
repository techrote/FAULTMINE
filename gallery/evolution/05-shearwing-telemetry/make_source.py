#!/usr/bin/env python3
"""Deterministic original drawing -> 24-bit Windows BMP, no external dependencies."""
import argparse
import hashlib
import math
import pathlib
import struct

W,H = 384,256
ROOT=pathlib.Path(__file__).resolve().parent
OUT=ROOT/'source.bmp'
C={
    'ground':(13,17,28),'slate':(22,31,45),'mid':(33,48,59),
    'wing':(217,223,190),'bone':(247,216,128),'cobalt':(68,182,192),
    'rust':(213,83,61),'violet':(112,89,142),'ink':(10,15,21),
}
pixels=bytearray(W*H*3)

def dot(x,y,c,alpha=255):
    if 0 <= x<W and 0<=y<H:
        i=(y*W+x)*3
        if alpha==255: pixels[i:i+3]=bytes(c)
        else:
            for k in range(3): pixels[i+k]=(pixels[i+k]*(255-alpha)+c[k]*alpha)//255

def line(a,b,c,thick=1):
    x0,y0=a; x1,y1=b
    n=max(abs(x1-x0),abs(y1-y0))*2+1
    for j in range(n+1):
        x=round(x0+(x1-x0)*j/n);y=round(y0+(y1-y0)*j/n)
        for dy in range(-thick//2,thick//2+1):
            for dx in range(-thick//2,thick//2+1):
                if dx*dx+dy*dy<=(thick/2+.5)**2: dot(x+dx,y+dy,c)

def polygon(p,c):
    min_y=max(0,min(pt[1] for pt in p)); max_y=min(H-1,max(pt[1] for pt in p))
    for y in range(min_y,max_y+1):
        intersections=[]
        for (ax,ay),(bx,by) in zip(p,p[1:]+p[:1]):
            if (ay<=y<by) or (by<=y<ay):
                intersections.append(ax+(y-ay)*(bx-ax)/(by-ay))
        intersections.sort()
        for aa,bb in zip(intersections[::2],intersections[1::2]):
            for x in range(max(0,math.ceil(aa)), min(W,math.ceil(bb))):dot(x,y,c)

def rect(x1,y1,x2,y2,c):
    for y in range(max(0,y1),min(H,y2)):
        for x in range(max(0,x1),min(W,x2)):dot(x,y,c)

def hashbyte(x,y,salt=0):
    k=((x*0x9e3779b1)^(y*0x85ebca77)^(salt*0xc2b2ae3d))&0xffffffff
    k^=k>>16; k=(k*0x7feb352d)&0xffffffff;k^=k>>15;k=(k*0x846ca68b)&0xffffffff;k^=k>>16
    return k&255

def render():
    for y in range(H):
        for x in range(W):
            d=((x-188)**2+(y-127)**2)//3200
            n=hashbyte(x,y,8)%6
            dot(x,y,(max(0,17-d//2+n//2),max(0,22-d//2+n//2),max(0,34-d//3+n//2)))
    # Print-maker's diagonal rule fields: two opposite navigational regions
    for y in range(21,237,11):
        for x in range(22,361,2):
            if (x + 3*y)%71 < 52 and (x<100 or x>305): dot(x,y,C['mid'])
    for x in range(24,365,22):
        line((x,14),(x-19,238),C['slate'])
    for x in range(20,366,43):
        for y in range(24,232,43):
            if hashbyte(x,y,13)>75:
                dot(x,y,C['violet']);dot(x+1,y,C['violet'])
    # Interfering strip of bright lines behind bird, tilted and interrupted
    for i in range(14):
        yy=53+i*11
        for x in range(11,374):
            y=yy-(x-192)//17
            if (x+13*i)%83<53 and (x<108 or x>291):
                if (x+i*7)%9<3:dot(x,y,C['cobalt'])
    # The wing body: huge single bird silhouette, swept from lower-left to upper-right.
    polygon([(42,158),(96,145),(142,119),(189,100),(247,69),(343,47),(318,78),(272,111),
             (245,125),(218,135),(185,144),(161,166),(141,183),(109,177),(81,168)],C['wing'])
    polygon([(155,148),(199,137),(237,125),(295,101),(265,128),(238,143),(221,158),(196,166),(177,169)],C['bone'])
    # Lower wing / second wing a sharply divergent triangle from main shoulder
    polygon([(165,145),(142,156),(121,191),(111,222),(154,206),(195,183),(219,164),(193,150)],C['cobalt'])
    polygon([(111,222),(146,203),(163,190),(149,203),(134,225)],C['rust'])
    # Dark void cuts through pale leading edge, feather bevels
    polygon([(238,78),(248,88),(308,64),(276,66)],C['ink'])
    for j in range(11):
        x=99+j*18
        y=149-(x-99)*3//10
        line((x,y),(x-28,y+24+j//4),C['ink'],1)
    for j in range(8):
        x=158+j*17
        y=177-(j*10)
        line((x,y),(x+26,y-23),C['rust'] if j%3==0 else C['mid'])
    # Tail vane, head and beak with clean negative cut
    polygon([(75,160),(33,179),(58,180),(85,172)],C['bone'])
    polygon([(79,162),(22,152),(50,164),(70,169)],C['rust'])
    polygon([(293,70),(332,60),(351,67),(332,72),(324,81),(306,87)],C['bone'])
    polygon([(333,65),(373,65),(337,72)],C['rust'])
    rect(324,67,327,70,C['ink'])
    # Spine of internal registration points, as if bird was a flight recorder
    for j in range(18):
        x=86+j*12
        y=155-(x-86)*22//175
        if 0<=x<W and 0<=y<H:
            dot(x,y,C['ink']); dot(x+1,y+1,C['rust'])
    # Interrupted flight path takes up negative space beneath the body
    for j in range(110):
        x=29+3*j
        y=208 + round(5*math.sin(j*0.075)) + j//20
        if j%17<10 and x<W:dot(x,y,C['bone'])
    for x,y,r in [(40,197,12),(328,157,24),(339,199,9)]:
        for degree in range(0,360,3):
            if (degree+2*x)%90<54:
                angle=math.radians(degree)
                dot(round(x+math.cos(angle)*r),round(y+math.sin(angle)*r),C['rust'] if r==24 else C['cobalt'])
        line((x-r-3,y),(x-r+3,y),C['mid'])
        line((x,y-r-3),(x,y-r+3),C['mid'])
    # sparse telemetry ticks in lower right
    for j in range(22):
        x=280+j*3; y=221+(j%5)
        line((x,y),(x,y-(7 if j%5==0 else 3)), C['bone'] if j%6==0 else C['violet'])
    rect(16,242,369,243,C['mid'])
    for j in range(0,353,13):
        line((16+j,239),(16+j,244),C['cobalt'] if j%3 else C['rust'])

def bmp():
    stride=(W*3+3)&~3; body=bytearray()
    for y in range(H-1,-1,-1):
        for x in range(W):
            i=(y*W+x)*3;r,g,b=pixels[i:i+3];body.extend((b,g,r))
        body.extend(b'\0'*(stride-W*3))
    size=54+len(body)
    return b'BM'+struct.pack('<IHHI',size,0,0,54)+struct.pack('<IiiHHIIiiII',40,W,H,1,24,0,len(body),2835,2835,0,0)+body

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--verify',action='store_true');ap.add_argument('--write',action='store_true');args=ap.parse_args()
    if not (args.write or args.verify): ap.error('choose --write or --verify')
    render(); result=bmp(); digest=hashlib.sha256(result).hexdigest()
    if args.verify:
        if not OUT.is_file() or OUT.read_bytes()!=result: raise SystemExit('ERROR: committed source.bmp differs from deterministic source')
        print('PASS exact source bytes',len(result),digest)
    else:
        if OUT.exists():raise SystemExit('ERROR: refusing overwrite of source.bmp')
        OUT.write_bytes(result); print('WROTE',OUT,len(result),digest)

if __name__=='__main__': main()
