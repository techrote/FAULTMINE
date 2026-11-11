#!/usr/bin/env python3
"""Independent stdlib APNG transport/pixel/replay test against pinned native ZIP.

The APNG is an offline 3-pane presentation, NOT a FAULTMINE engine export.
"""
from __future__ import annotations
import base64
import hashlib
import json
import re
from pathlib import Path
import struct
import sys
import tempfile
from zipfile import ZipFile
import zlib

import compare_native as auth
import make_synchronized_apng as player


def read_apng(data: bytes) -> list[bytes]:
    if not data.startswith(player.PNG_MAGIC):
        raise ValueError('Not PNG')
    pos=8
    frames=[]
    active=None
    expected_seq=0
    have_header=have_animation=have_end=False
    while pos<len(data):
        if pos+12>len(data):raise ValueError('Truncated chunk')
        n=struct.unpack_from('>I',data,pos)[0]
        kind=data[pos+4:pos+8]
        payload=data[pos+8:pos+8+n]
        crc=struct.unpack_from('>I',data,pos+8+n)[0]
        assert len(payload)==n and zlib.crc32(kind+payload)&0xffffffff==crc,(kind,pos)
        pos+=12+n
        if kind==b'IHDR':
            assert not have_header
            assert payload==struct.pack('>IIBBBBB',player.WIDTH,player.HEIGHT,8,6,0,0,0)
            have_header=True
        elif kind==b'acTL':
            assert have_header and not have_animation
            assert payload==struct.pack('>II',64,0)
            have_animation=True
        elif kind==b'fcTL':
            if active is not None:frames.append(active)
            assert len(payload)==26
            seq,width,height,dx,dy,num,den,dispose,blend=struct.unpack('>IIIIIHHBB',payload)
            assert seq==expected_seq and (width,height,dx,dy,num,den,dispose,blend)==(player.WIDTH,player.HEIGHT,0,0,player.FRAME_MS,1000,0,0)
            expected_seq+=1;active=bytearray()
        elif kind==b'IDAT':
            assert len(frames)==0 and active is not None
            active.extend(payload)
        elif kind==b'fdAT':
            assert active is not None and len(frames)>0 and len(payload)>=4
            seq=struct.unpack_from('>I',payload)[0]
            assert seq==expected_seq;expected_seq+=1
            active.extend(payload[4:])
        elif kind==b'IEND':
            assert not payload and pos==len(data)
            if active is not None:frames.append(active)
            have_end=True
            break
        else:raise ValueError('Unexpected APNG chunk '+repr(kind))
    assert have_header and have_animation and have_end and len(frames)==64
    rendered=[]
    stride=player.WIDTH*4
    for blob in frames:
        lines=zlib.decompress(blob)
        assert len(lines)==player.HEIGHT*(stride+1)
        assert all(lines[row*(stride+1)]==0 for row in range(player.HEIGHT))
        rendered.append(b''.join(lines[row*(stride+1)+1:(row+1)*(stride+1)] for row in range(player.HEIGHT)))
    return rendered


def verify(archive: Path) -> None:
    with tempfile.TemporaryDirectory() as dirname:
        path=Path(dirname)/'native-review.png'
        result=player.build(archive,path)
        data=path.read_bytes()
        assert hashlib.sha256(data).hexdigest()==result['sha256']
        assert result['frames']==64 and result['angle_native_frames']==36 and result['seat_native_frames']==32
        assert tuple(result['editorial_native_indices'])==auth.SEAT_STUDY
        html=Path(dirname)/'review.html'
        auth.build(archive,html)
        emitted=re.search(r'const seatStudyMs=(\[[0-9,]+\]);', html.read_text('utf-8'))
        assert emitted, 'Missing generated browser exposure table'
        exposures=json.loads(emitted.group(1))
        assert exposures==list(auth.SEAT_STUDY_EXPOSURES)
        def from_browser_table(ms: int) -> int:
            for index, exposure in zip(auth.SEAT_STUDY, exposures):
                if ms < exposure:return index
                ms -= exposure
            return 31
        assert all(player.seat_study_at(t)==from_browser_table(t) for t in (*player.TIMES, 3200)), 'Browser and APNG native frame schedules diverged'
        pixels=read_apng(data)
        with ZipFile(archive) as z:
            inputs={spec['key']:[auth.decode_rgba(base64.b64decode(frame['url'].split(',',1)[1]),spec['width'],spec['height']) for frame in auth.read_family(z,spec)] for spec in auth.SPECS}
        seen_angle=set();seen_seat=set();seen_edit=set()
        for n,t in enumerate(player.TIMES):
            a=min(35,t*12//1000);b=min(31,t//100);c=player.seat_study_at(t)
            seen_angle.add(a);seen_seat.add(b);seen_edit.add(c)
            for (x,y,w,h),truth in zip(player.PANELS,(inputs['angle'][a],inputs['seat'][b],inputs['seat'][c])):
                actual=b''.join(pixels[n][(y+row)*player.WIDTH*4+x*4:(y+row)*player.WIDTH*4+(x+w)*4] for row in range(h))
                assert actual==truth,(n,t,x,a,b,c)
        assert seen_angle==set(range(36)) and seen_seat==set(range(32))
        assert seen_edit==set(auth.SEAT_STUDY)
        # The self-running APNG needs legible, unambiguous panel names and
        # genuine live native-frame indices; captions occupy only y >= 238.
        assert (player.WIDTH, player.HEIGHT)==(1024,272)
        background=b'\x0c\x11\x19\xff'
        def caption_slice(frame: bytes, x: int) -> bytes:
            return b''.join(frame[y*player.WIDTH*4+x*4:
                                  y*player.WIDTH*4+(x+245)*4]
                            for y in range(238,266))
        for x in (8,336,680):
            panels=[caption_slice(pixels[i],x) for i in (0,30,63)]
            assert all(any(p[j:j+4]!=background
                           for j in range(0,len(p),4)) for p in panels), (
                           'Missing panel text',x)
            assert panels[0]!=panels[2], ('Stale native frame caption',x)
        # A title panel must not be mistakenly copied to all three tracks.
        assert len({caption_slice(pixels[30],x) for x in (8,336,680)})==3
        try:
            player.build(archive,path)
        except FileExistsError:pass
        else:raise AssertionError('Clobbered prior preview')
        assert path.read_bytes()==data,'Output collision modified existing APNG'
        print('PASS: 64-frame APNG structural/CRC and exact native source pixels, all 36/32/19 presentation indices, live caption identity; collision safe')


if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('usage: python test_synchronized_apng.py FOUR-FAMILY-REVIEW.zip')
    verify(Path(sys.argv[1]))
