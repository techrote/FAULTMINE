#!/usr/bin/env python3
"""SEISMIC DEBT -- deterministic self-authored 384x240 24-bit BMP; stdlib only.
No random input, external font, image, clock, or floating-point raster operation.
Usage: python make_source.py --write | --verify
"""
import argparse
import hashlib
import pathlib
import struct

W, H = 384, 240
ROOT = pathlib.Path(__file__).resolve().parent
DEST = ROOT / 'source.bmp'
C = {
    'void': (11, 16, 27), 'grid': (28, 42, 52), 'grid2': (42, 61, 67),
    'chalk': (224, 220, 181), 'signal': (100, 208, 193), 'secondary': (162, 174, 146),
    'rust': (218, 86, 60), 'ember': (255, 168, 83), 'shadow': (17, 28, 38),
}
pixels = bytearray(C['void'] * (W * H))

def dot(x, y, color):
    if 0 <= x < W and 0 <= y < H:
        i = (y*W+x)*3
        pixels[i:i+3] = bytes(color)

def rect(x0, y0, x1, y1, col):
    for y in range(max(y0,0), min(y1,H)):
        for x in range(max(x0,0), min(x1,W)):
            dot(x,y,col)

def line(x0,y0,x1,y1,col,weight=1):
    dx,dy=abs(x1-x0),abs(y1-y0)
    sx=1 if x0<x1 else -1
    sy=1 if y0<y1 else -1
    err=dx-dy
    while True:
        for oy in range(-(weight//2), (weight+1)//2):
            for ox in range(-(weight//2), (weight+1)//2): dot(x0+ox,y0+oy,col)
        if x0==x1 and y0==y1: break
        e2=2*err
        if e2>-dy: err-=dy; x0+=sx
        if e2<dx: err+=dx; y0+=sy

def tri(v, period):
    t=v%period
    return (t if t<period//2 else period-t)*2-period//2

# Tiny purpose-built all-caps telemetry glyphs, not an imported asset.
FONT={
'S':('111','100','110','001','111'), 'E':('111','100','110','100','111'),
'I':('111','010','010','010','111'), 'M':('101','111','111','101','101'),
'C':('111','100','100','100','111'), 'D':('110','101','101','101','110'),
'B':('110','101','110','101','110'), 'T':('111','010','010','010','010'),
'L':('100','100','100','100','111'), 'P':('110','101','110','100','100'),
'H':('101','101','111','101','101'), 'R':('110','101','110','101','101'),
'Y':('101','101','010','010','010'),
'A':('010','101','111','101','101'), 'N':('101','111','111','111','101'),
'0':('111','101','101','101','111'), '1':('010','110','010','010','111'),
'2':('111','001','111','100','111'), '3':('111','001','111','001','111'),
'4':('101','101','111','001','001'), '5':('111','100','111','001','111'),
'6':('111','100','111','101','111'), '7':('111','001','001','001','001'),
'8':('111','101','111','101','111'), '9':('111','101','111','001','111'),
'/':('001','001','010','100','100'), '-':('000','000','111','000','000'),
' ':('000','000','000','000','000'), '.':('000','000','000','000','010')}
def label(s,x,y,col=C['chalk'],scale=1):
    for ch in s:
        for j,row in enumerate(FONT[ch]):
            for k,c in enumerate(row):
                if c=='1': rect(x+k*scale,y+j*scale,x+(k+1)*scale,y+(j+1)*scale,col)
        x+=4*scale

# Technical ground: two scales of horizontal/vertical divisions, but not flat filler.
for x in range(15,374,18):
    for y in range(21,212,3):
        dot(x,y,C['grid'] if x%36 else C['grid2'])
for y in (30,74,118,162,206):
    line(17,y,367,y,C['grid2'])
for y in (52,96,140,184):
    for x in range(18,369,8):
        dot(x,y,C['grid2'])
# Four scoring lanes with distinct amplitude envelopes and frequency structures.
centres=[52,96,140,184]
for band,cy in enumerate(centres):
    # left channel terminals and calibration discontinuities
    rect(13,cy-9,18,cy+10,C['grid2']);rect(13,cy-1,20,cy+2,C['chalk'])
    label('0'+str(band),23,cy-14,C['secondary'])
    for x in range(47,352):
        if band==0:
            amp=2+max(0,12-abs(x-165)//3)
            y=cy + tri(x*3,40)*amp//20
        elif band==1:
            amp=3+max(0,33-abs(x-178)//3)
            y=cy + tri(x*7+(x//17)*5,26)*amp//15
        elif band==2:
            amp=3+max(0,19-abs(x-210)//5) + max(0,8-abs(x-290)//6)
            y=cy + tri(x*4+(x//31)*12,38)*amp//17
        else:
            amp=2+max(0,16-abs(x-269)//4)
            y=cy + tri(x*5+17,31)*amp//17
        if x==47: prev_y=y
        col=C['chalk'] if band in (0,2) else C['signal']
        if x%7==0: line(x-1,prev_y-1,x,y-1,C['shadow'],3)
        line(x-1,prev_y,x,y,col,2 if band in (1,2) else 1)
        prev_y=y
    # numerical notches, channel-specific dense dots at left
    for x in range(58,365,45):
        rect(x,cy-17,x+2,cy-15,C['secondary'])
    label(str(band+1)+'7',354,cy-14,C['ember'])

# Repetition after the quake: ghost waveform from before the fracture, offset upward and partial.
for x in range(217,341):
    if (x//8)%4==0: continue
    wave=tri(x*7+31,29)
    y=92+wave*7//19
    dot(x,y,C['rust'])

# Major fault cut: severe oblique black fissure that deletes parts of all four recordings.
def fault_x(y): return 196-(y//12)*2+((y//33)%2)*4
for y in range(18,213):
    cx=fault_x(y)
    for x in range(cx-4,cx+5): dot(x,y,C['void'])
    dot(cx-5,y,C['rust'])
    if y%3==0: dot(cx+6,y,C['ember'])
# A clipped, large rust polygon below the fault, like a snapped seismograph needle.
for j in range(31):
    y=209+j
    line(163-j//4,y,169+j//5,y,C['rust'] if j%3 else C['ember'])

# Cropped, highly readable instrument metadata: an overprint rather than UI chrome.
rect(12,9,372,11,C['grid2'])
label('SEISMIC DEBT',19,14,C['chalk'],2)
label('11/26',320,14,C['ember'],2)
label('DEPTH / 04',18,219,C['secondary'])
label('REPLAY 0',307,219,C['chalk'])
rect(14,214,367,215,C['grid2'])
for i in range(8):
    x=145+i*8
    rect(x,222,x+3,227,C['rust'] if i%3==0 else C['grid2'])

# Standard 24-bit BMP, 4-byte aligned bottom-up rows.
stride=((W*3+3)//4)*4
out=bytearray()
out+=b'BM'+struct.pack('<IHHI',54+stride*H,0,0,54)
out+=struct.pack('<IiiHHIIiiII',40,W,H,1,24,0,stride*H,2835,2835,0,0)
for y in range(H-1,-1,-1):
    for x in range(W):
        r,g,b=pixels[(y*W+x)*3:(y*W+x)*3+3]
        out+=bytes((b,g,r))
    out+=b'\x00'*(stride-W*3)

enc=hashlib.sha256(out).hexdigest()
raw=bytearray()
for i in range(0,len(pixels),3): raw+=pixels[i:i+3]+b'\xff'
identity=hashlib.sha256(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',W,H)+raw).hexdigest()

def main():
    p=argparse.ArgumentParser(); x=p.add_mutually_exclusive_group(required=True)
    x.add_argument('--write',action='store_true'); x.add_argument('--verify',action='store_true')
    a=p.parse_args()
    if a.write:
        if DEST.exists() and DEST.read_bytes()!=out:
            p.error('refusing to overwrite changed source.bmp')
        if not DEST.exists(): DEST.write_bytes(out)
    elif not DEST.exists() or DEST.read_bytes()!=out:
        p.error('BMP missing or mismatched (write only with --write)')
    print('bmp_sha256='+enc)
    print('source_identity='+identity)
    print('dimensions=384x240')

if __name__=='__main__': main()
