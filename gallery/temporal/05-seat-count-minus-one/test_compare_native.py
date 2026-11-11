#!/usr/bin/env python3
"""Exact authentic Windows native evidence contract and negative tests for dual review.

Run: python test_compare_native.py PATH/TO/FOUR-FAMILY-REVIEW.zip
Requires only Python standard library; no native Windows acceptance is implied.
"""
import base64
import json
from pathlib import Path
import re
import sys
import tempfile
from zipfile import ZipFile, ZIP_DEFLATED
import compare_native as subject


def run(source: Path) -> None:
    with tempfile.TemporaryDirectory() as temp:
        dest = Path(temp)/'review.html'
        result = subject.build(source,dest)
        assert result['angle_frames']==36 and result['seat_frames']==32
        page = dest.read_text('utf-8')
        embedded = re.search(r'const images = (\{.*?\});', page)
        assert embedded, 'No exact archived images embedded'
        sets = json.loads(embedded.group(1))
        assert list(sets)==['angle','seat']
        with ZipFile(source) as z:
            source_entries = {
                n:z.read(n) for n in z.namelist()
                if any('/CANONICAL-REVIEW-EVIDENCE/'+s['folder']+'/' in n
                       for s in subject.SPECS)
            }
            for spec in subject.SPECS:
                for index, row in enumerate(sets[spec['key']]):
                    name = subject.unique(list(source_entries),
                        'CANONICAL-REVIEW-EVIDENCE/'+spec['folder']+'/'+spec['basename']+f'_{index:06d}.png')
                    assert row['index']==index
                    assert base64.b64decode(row['url'].split(',',1)[1])==source_entries[name]
        # Only the temporal presentation can change: immutable source-exact
        # 32-frame authored playback remains the default, with the same
        # 19-frame editorial selection already published by author #54.
        cut_match = re.search(r'const seatStudy=(\[[0-9,]+\]);', page)
        assert cut_match, 'No optional Seat editorial cut'
        cut = json.loads(cut_match.group(1))
        assert cut == [0,1,2,4,6,8,9,12,13,14,18,19,21,23,24,27,28,29,31]
        assert len(set(cut)) == len(cut) and cut == sorted(cut)
        exposure = [500 if n == 0 else 545 if n == 31 else
                    155 if n < 9 else 115 for n in cut]
        assert sum(exposure) == 3200, 'Editorial timing no longer synchronized'
        assert '<option value="authored">Authored 32 frames · 10 fps' in page
        assert "seatCadence.onchange=()=>{drawContacts();select(ms)}" in page
        assert "const contactTimes=[0,450,900,1350,1800,2250,2700,3200]" in page
        assert "button.dataset.time=String(t)" in page
        assert "const index=frameAt(type,t)" in page
        assert "const active=contactTimes.reduce((best,t,index)=>ms>=t?index:best,0)" in page
        # These measurements justify the revision using actual decoded native
        # pixels, not guessed frame-level aesthetic labels.
        seat_spec = next(s for s in subject.SPECS if s['key'] == 'seat')
        def seat_rgba(n):
            name = subject.unique(list(source_entries),
                'CANONICAL-REVIEW-EVIDENCE/seat-05-release/seat-count_'+f'{n:06d}.png')
            return subject.decode_rgba(source_entries[name],
                seat_spec['width'], seat_spec['height'])
        for left,right,wanted_percent in ((26,27,7.84),(27,28,4.48),(28,29,42.85)):
            a,b = seat_rgba(left),seat_rgba(right)
            modified = sum(a[k:k+3] != b[k:k+3] for k in range(0,len(a),4))
            actual = 100 * modified / (seat_spec['width']*seat_spec['height'])
            assert abs(actual-wanted_percent) < .01, (left,right,actual)
        try:
            subject.build(source,dest)
        except FileExistsError:
            pass
        else:
            raise AssertionError('Stale HTML was overwritten')
        def negative(changer, expected):
            altered = Path(temp)/'bad.zip'
            with ZipFile(altered,'w',ZIP_DEFLATED) as z:
                for path,data in source_entries.items():
                    if changer is not None:
                        replacement = changer(path,data)
                        if replacement is None:
                            continue
                        data = replacement
                    z.writestr(path,data)
            try:
                subject.build(altered,Path(temp)/'candidate.html')
            except (ValueError, KeyError) as exc:
                assert expected in str(exc), (expected, str(exc))
            else:
                raise AssertionError('Expected rejection of '+expected)
        negative(lambda n,d: d+b'!' if n.endswith('angle-04-release/native-cli-replay-audit.json') else d, 'angle Windows-native audit receipt changed')
        negative(lambda n,d: d[:-1]+bytes((d[-1]^1,)) if n.endswith('seat-count_000021.png') else d, 'seat encoded transport mismatch: 21')
        negative(lambda n,d: None if n.endswith('angle-departure_000035.png') else d, 'Expected exactly one Windows native artifact')
        print('PASS: 68/68 byte-for-byte embedded original PNGs, two pinned receipts, decoded native identity checks, refuse-overwrite, three corrupt/missing asset negative guards')


if __name__ == '__main__':
    if len(sys.argv)!=2:
        raise SystemExit('usage: python test_compare_native.py WINDOWS-FOUR-FAMILY-REVIEW.zip')
    run(Path(sys.argv[1]))
