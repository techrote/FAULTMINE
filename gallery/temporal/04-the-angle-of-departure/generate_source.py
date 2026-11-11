#!/usr/bin/env python3
"""Rebuild the original source.bmp with Python stdlib only; no external assets."""
from pathlib import Path
import struct, hashlib

W,H=320,192
DARK=(11,17,28); PLATE=(212,215,191); CREAM=(245,224,181)
RUST=(221,93,66); GOLD=(246,168,82); TEAL=(78,191,181)
PALE=(159,210,207); SHADOW=(26,48,63); GREY=(69,90,95)
pixels=[DARK]*(W*H)

def dot(x,y,c):
    if 0<=x<W and 0<=y<H: pixels[y*W+x]=c

def rect(x0,y0,x1,y1,c):
    for y in range(max(0,y0),min(H,y1)):
        for x in range(max(0,x0),min(W,x1)): dot(x,y,c)

def line(x0,y0,x1,y1,c,width=1):
    dx=abs(x1-x0); dy=-abs(y1-y0); sx=1 if x0<x1 else -1; sy=1 if y0<y1 else -1; err=dx+dy
    while True:
        for ty in range(-(width//2),width-width//2):
            for tx in range(-(width//2),width-width//2): dot(x0+tx,y0+ty,c)
        if x0==x1 and y0==y1: break
        e2=2*err
        if e2>=dy: err+=dy; x0+=sx
        if e2<=dx: err+=dx; y0+=sy

def inside(points,x,y):
    active=False
    for i,(xa,ya) in enumerate(points):
        xb,yb=points[(i+1)%len(points)]
        if (ya>y)!=(yb>y) and x<xa+(y-ya)*(xb-xa)/(yb-ya): active=not active
    return active

def poly(points,c):
    for y in range(max(0,min(q[1] for q in points)),min(H,max(q[1] for q in points)+1)):
        for x in range(W):
            if inside(points,x,y): dot(x,y,c)

def frame(points,c,width=1):
    for i in range(len(points)):
        line(*points[i],*points[(i+1)%len(points)],c,width)

for x in range(14,312,19): line(x,8,x,184,SHADOW if x%2 else GREY)
for y in range(13,188,17): line(9,y,310,y,SHADOW)
for y in (8,183): line(10,y,310,y,GREY)
for x in (10,309): line(x,8,x,183,GREY)
A=[(33,39),(158,14),(250,46),(104,107)]
B=[(67,128),(226,51),(295,106),(144,177)]
C=[(42,157),(119,68),(171,93),(96,180)]
poly(A,PLATE);frame(A,GOLD,2)
poly(B,RUST);frame(B,CREAM,2)
poly(C,TEAL);frame(C,PALE,2)
maskA=[False]*(W*H);maskB=[False]*(W*H);maskC=[False]*(W*H)
for pts,target in ((A,maskA),(B,maskB),(C,maskC)):
    for y in range(H):
        for x in range(W): target[y*W+x]=inside(pts,x,y)
for y in range(H):
    for x in range(W):
        p=y*W+x
        if maskA[p] and (3*x+5*y)%29<2: dot(x,y,GREY)
        if maskB[p] and (x+2*y)%18<2: dot(x,y,GOLD)
        if maskC[p] and (2*x-y)%21<2: dot(x,y,SHADOW)
for i in range(13):
    x=36+18*i; y=25+(i*7)%21
    line(x,y,x+13,y-3,CREAM if i%3==0 else RUST)
    if i%2==0: rect(x+15,y-2,x+18,y+1,GOLD)
VOID=[(114,106),(180,58),(232,94),(159,138)]
INNER=[(138,105),(181,74),(208,95),(161,123)]
poly(VOID,DARK);frame(VOID,CREAM,2);frame(INNER,GOLD,2)
line(119,108,227,93,TEAL,2);line(153,132,183,62,RUST,2)
for i in range(4):
    s=i*13; shape=[(17+s,116-i*12),(29+s,111-i*13),(49+s,128-i*12),(35+s,134-i*12)]
    poly(shape,DARK if i%2 else CREAM);frame(shape,RUST)
for i in range(5):
    s=i*14; shape=[(222+s,131+i*4),(234+s,125+i*4),(245+s,140+i*4),(231+s,146+i*4)]
    poly(shape,TEAL if i%2 else PLATE);frame(shape,DARK)
for i in range(9):
    x=22+13*i; y=164+(i%3)*5
    line(x,y,x+5,y,CREAM if i%2 else GOLD);line(x,y,x,y+4,GOLD)
for i in range(7):
    x=260+i*6; y=32+(i%3)*5
    line(x,y,x+3,y,TEAL);dot(x,y+3,CREAM)
for x,y,c in ((22,15,RUST),(286,18,TEAL),(288,171,GOLD)):
    rect(x,y,x+8,y+8,c);rect(x+2,y+2,x+6,y+6,DARK)

def encode():
    stride=(W*3+3)&~3
    data=bytearray()
    for y in range(H-1,-1,-1):
        for x in range(W):
            r,g,b=pixels[y*W+x];data.extend((b,g,r))
        data.extend(bytes(stride-W*3))
    header=b'BM'+struct.pack('<IHHI',54+len(data),0,0,54)+struct.pack('<IiiHHIIiiII',40,W,H,1,24,0,len(data),2835,2835,0,0)
    return header+data

if __name__=='__main__':
    original=Path(__file__).with_name('source.bmp')
    data=encode()
    assert hashlib.sha256(data).hexdigest()=='7fdf2e80da19a28fe41b15d25599e790aee6ca231ae0fd580df87ad66daecd52', 'source generator regression'
    if original.exists() and original.read_bytes()!=data:
        raise SystemExit('Refusing to overwrite mismatched immutable source.bmp')
    original.write_bytes(data)
    print('PASS: original 320x192 BMP reproduced exactly')
