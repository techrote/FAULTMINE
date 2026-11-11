#!/usr/bin/env python3
"""Full authored-frame SVG contract against verified native archive (stdlib)."""
from __future__ import annotations
import base64
from pathlib import Path
import sys
import tempfile
from xml.etree import ElementTree
from zipfile import ZipFile
import full_sequence_contact as subject
import compare_native as native


def test(archive: Path) -> None:
    ns = '{http://www.w3.org/2000/svg}'
    with tempfile.TemporaryDirectory() as tmp:
        output = Path(tmp) / 'review.svg'
        result = subject.build(archive, output)
        assert result['seat']==32 and result['angle']==36
        assert result['comparator']==6
        assert result['late_change_pct']==[7.84,4.48,42.85]
        root = ElementTree.parse(output).getroot()
        assert root.tag==ns+'svg'
        elements=root.findall(ns+'image')
        assert len(elements)==38
        with ZipFile(archive) as z:
            families={s['key']:native.read_family(z,s) for s in native.SPECS}
        selected=families['seat'] + [families['angle'][i] for i in subject.ANGLE_SAMPLES]
        assert len({r['id'] for r in selected})==38
        for node, row in zip(elements, selected):
            assert node.attrib['href']==row['url']
            assert base64.b64decode(node.attrib['href'].split(',',1)[1],validate=True)==base64.b64decode(row['url'].split(',',1)[1],validate=True)
        labels='\n'.join((node.text or '') for node in root.findall(ns+'text'))
        assert 'UNADMITTED' in labels and 'frame 31' in labels
        assert 'ANGLE #50' in labels and 'RGB-channel change' in labels
        try:
            subject.build(archive, output)
        except FileExistsError:
            pass
        else:
            raise AssertionError('Changed/replaced existing owner review SVG')
        try:
            subject.rgb_pixel_change(b'\x00\x00\x00\xff', b'\xff')
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted incompatible canonical buffers')
    print('PASS: 68 verified authentic frames; 32 authored Seat ordered + six native Angle comparator images; percent change and fail-on-overwrite')


if __name__=='__main__':
    if len(sys.argv)!=2:
        raise SystemExit('usage: python test_full_sequence_contact.py PINNED-REVIEW.zip')
    test(Path(sys.argv[1]))
