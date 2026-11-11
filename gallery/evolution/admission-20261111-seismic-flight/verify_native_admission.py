#!/usr/bin/env python3
"""Fail-closed, read-only replay of *archived* FM-014 evidence; does not mine or render.

Usage: python verify_native_admission.py FOUR_FAMILY_ZIP EVOLUTION_REVIEW_ZIP
"""
import hashlib, json, struct, sys, zipfile, zlib
from pathlib import Path

SPEC = {
 'shearwing': (96,70,96,65,(384,256),'20c1a2a55064127311872552129f9854a677d3577904d60aeff280ed4eb21033','6f149f34cb9eca441c4206de1b5e7b0f035cab3b2ad958e3401f5a1cef2d9c50'),
 'seismic': (128,85,125,101,(384,240),'85e2faa021132bb803192c2471d2b453f064c71522cfc5bc6beef8551f18cf6d','492f844f1dc21cd3abf4516aa7c383fc276fbfda72205ae628b03a6b52a443e2')
}
PREFIX='FAULTMINE-1.0.0-win64/CANONICAL-REVIEW-EVIDENCE/shearwing-05/'
def check(ok, message):
 if not ok: raise ValueError(message)
def digest(b): return hashlib.sha256(b).hexdigest()

def pixel_identity(data, wh):
 check(data.startswith(b'\x89PNG\r\n\x1a\n'),'bad PNG signature')
 pos=8; ihdr=None; parts=[]; done=False
 while pos<len(data):
  check(pos+12<=len(data),'truncated PNG')
  n=int.from_bytes(data[pos:pos+4],'big')
  check(n<8_000_000 and pos+n+12<=len(data),'bad PNG chunk length')
  kind=data[pos+4:pos+8]; chunk=data[pos+8:pos+8+n]
  crc=int.from_bytes(data[pos+8+n:pos+12+n],'big')
  check((zlib.crc32(kind+chunk)&0xffffffff)==crc,'PNG CRC mismatch')
  if kind==b'IHDR':
   check(ihdr is None and n==13,'bad IHDR')
   ihdr=struct.unpack('>IIBBBBB',chunk)
  if kind==b'IDAT': parts.append(chunk)
  if kind==b'IEND':
   check(n==0 and pos+12+n==len(data),'bad IEND')
   done=True
  pos+=n+12
 w,h=wh
 check(done and ihdr==(w,h,8,6,0,0,0),'wrong RGBA8 PNG size/format')
 raw=zlib.decompress(b''.join(parts)); stride=w*4
 check(len(raw)==h*(stride+1),'bad decompressed scanlines')
 prev=bytearray(stride); out=bytearray()
 for row in range(h):
  start=row*(stride+1); mode=raw[start]
  check(mode in range(5),'bad PNG filter')
  line=bytearray(raw[start+1:start+1+stride])
  for x in range(stride):
   a=line[x-4] if x>=4 else 0
   b=prev[x]; c=prev[x-4] if x>=4 else 0
   if mode==0: p=0
   elif mode==1: p=a
   elif mode==2: p=b
   elif mode==3: p=(a+b)//2
   else:
    v=a+b-c; da,db,dc=abs(v-a),abs(v-b),abs(v-c)
    p=a if da<=db and da<=dc else b if db<=dc else c
   line[x]=(line[x]+p)&255
  out.extend(line);prev=line
 return digest(b'FAULTMINE-SOURCE-RGBA8-v1'+struct.pack('<II',w,h)+out)

def audit(blob,family):
 count,reps,unique,selected,wh,pinned,winner=SPEC[family]
 check(digest(blob)==pinned,f'{family}: semantic manifest bytes differ')
 doc=json.loads(blob); rows=doc['candidates']
 check(doc['complete'] is True and doc['cancelled'] is False and not doc['failures'],'incomplete batch')
 check(doc['render_mode']=='canonical' and doc['frame_index']==0,'not frame-zero canonical')
 check(doc['index_begin']==0 and doc['index_end_exclusive']==count and len(rows)==count,'wrong range')
 check(doc['descriptor_version']==1 and doc['near_duplicate_threshold']==8192,'wrong dedupe policy')
 genomes={};pixels={};near=[]
 for i,row in enumerate(rows):
  check(row['descendant_index']==i,'out-of-order indices')
  gi,pi=row['genome_identity'],row['pixel_identity']
  check(row['genome_representative_index']==genomes.setdefault(gi,i),'wrong genome rep')
  first=pixels.setdefault(pi,i)
  check(row['pixel_representative_index']==first,'wrong pixel rep')
  d=row['descriptor']
  check(len(d)==24 and all(type(x)==int and 0<=x<=65535 for x in d),'bad descriptor')
  if first==i:
   representative=next((idx for idx,desc in near if sum(abs(x-y) for x,y in zip(d,desc))<=8192),i)
   if representative==i: near.append((i,d))
   check(row['near_representative_index']==representative,'wrong near rep')
 check(len(genomes)==count and len(pixels)==unique and len(near)==reps,'wrong actual counts')
 check(rows[selected]['selected'] and rows[selected]['pixel_identity']==winner,'wrong selected winner')
 return doc

def verify(four,review):
 with zipfile.ZipFile(four) as native,zipfile.ZipFile(review) as desk:
  for family in ('shearwing','seismic'):
   count,reps,unique,selected,wh,_,winner=SPEC[family]
   blob=desk.read(family+'-batch.fmbatch.json')
   doc=audit(blob,family)
   if family=='shearwing':
    check(native.read(PREFIX+'batch.fmbatch.json')==blob,'native manifest transport mismatch')
    for row in doc['candidates']:
     i=row['descendant_index']
     png=native.read(PREFIX+f'candidates/candidate_{i:020}.png')
     check(pixel_identity(png,wh)==row['pixel_identity'],f'Shearwing native candidate {i} mismatch')
    png=native.read(PREFIX+f'selected/candidate_{selected:020}.png')
    check(png==desk.read('shearwing-winner.png'),'Shearwing winner transport mismatch')
   else: png=desk.read('seismic-winner.png')
   check(pixel_identity(png,wh)==winner,f'{family}: winner decoded pixels mismatch')
   print(f'PASS {family}: {count}/{count}, failures=0, unique-pixels={unique}, near-reps={reps}, winner=#{selected}')
 print('PASS: all 96 Shearwing native cache PNGs independently decoded and reconciled')

if __name__=='__main__':
 if len(sys.argv)!=3: sys.exit('usage: python verify_native_admission.py FOUR_FAMILY_ZIP EVOLUTION_REVIEW_ZIP')
 try: verify(Path(sys.argv[1]),Path(sys.argv[2]))
 except (ValueError,KeyError,IndexError,zipfile.BadZipFile,zlib.error) as e: sys.exit('FAIL: '+str(e))
