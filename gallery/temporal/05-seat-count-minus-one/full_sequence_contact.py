#!/usr/bin/env python3
"""Source-verified, single-sheet review of all authored Seat frames versus Angle.

Only genuine archived Windows Release CPU PNGs are embedded. Presentation is
noncanonical, offline, noninteractive and never replaces the authored timeline.
Requires the adjacent compare_native.py (stdlib only), not Pillow or a browser.
"""
from __future__ import annotations
import argparse
import base64
from html import escape
from pathlib import Path
from zipfile import ZipFile
import compare_native as native


ANGLE_SAMPLES = (0, 7, 14, 21, 28, 35)


def rgb_pixel_change(left: bytes, right: bytes) -> float:
    """Percent of pixels where at least one RGB channel differs (ignore alpha)."""
    if len(left) != len(right) or len(left) % 4:
        raise ValueError('Canonical RGBA8 buffers are incompatible')
    if not left:
        raise ValueError('Empty canonical image')
    return 100.0 * sum(left[i:i+3] != right[i:i+3]
                       for i in range(0, len(left), 4)) / (len(left) // 4)


def build(archive: Path, dest: Path) -> dict:
    with ZipFile(archive) as z:
        # The existing PR-head verifier rejects wrong native receipts, order,
        # transport hashes, decoded pixel identities and duplicate output IDs.
        all_frames = {s['key']: native.read_family(z, s) for s in native.SPECS}
    assert len(all_frames['seat']) == 32 and len(all_frames['angle']) == 36
    w, h, gap, top, title = 336, 224, 14, 65, 65
    tile_w, tile_h = w + gap, h + title + gap
    size_x = 8 * tile_w + gap
    size_y = top + 5 * tile_h + 60
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{size_x}" height="{size_y}" viewBox="0 0 {size_x} {size_y}" role="img" aria-label="Original 32 Seat frames in exact order, six Angle comparison frames, native-image labels and frame-change percentages">',
             f'<rect width="{size_x}" height="{size_y}" fill="#0d1520"/>',
             '<style>text{font-family:ui-monospace,Consolas,monospace;fill:#e1e8f0} .small{font-size:15px}.head{font-size:20px}.meta{font-size:13px;fill:#aabdd0}</style>',
             '<text x="14" y="27" class="head">SEAT #54 — complete 32 native frames / 10 fps — UNADMITTED</text>',
             '<text x="14" y="49" class="meta">Δ: percent of full-resolution pixels with any RGB-channel change from prior authored frame. No interpolation, recolouring or proxy pixels.</text>']
    percent = {}
    previous = None
    seat_spec = next(s for s in native.SPECS if s['key']=='seat')
    for index, item in enumerate(all_frames['seat']):
        blob = base64.b64decode(item['url'].split(',', 1)[1], validate=True)
        pixels = native.decode_rgba(blob, seat_spec['width'], seat_spec['height'])
        changed = None if previous is None else rgb_pixel_change(previous, pixels)
        percent[index] = changed
        previous = pixels
        x = gap + (index % 8) * tile_w
        y = top + (index // 8) * tile_h
        # Expose plateaus for owner, without implying aesthetic verdict.
        line = '#e8a667' if index in (27,28) else '#506476'
        parts.append(f'<rect x="{x-2}" y="{y-2}" width="{w+4}" height="{h+4}" fill="none" stroke="{line}" stroke-width="2"/>')
        parts.append(f'<image x="{x}" y="{y}" width="{w}" height="{h}" href="{item["url"]}" image-rendering="pixelated"/>')
        cap = f'frame {index:02d} | {index*100:04d}ms | Δ ' + (f'{changed:.2f}%' if changed is not None else 'baseline')
        parts.append(f'<title>Seat native frame {index}; identity {escape(item["id"])}; RGB-pixel change {changed}</title>')
        parts.append(f'<text x="{x}" y="{y+h+22}" class="small">{escape(cap)}</text>')
        parts.append(f'<text x="{x}" y="{y+h+43}" class="meta">RGBA8 {escape(item["id"][:16])}…</text>')
    y = top + 4*tile_h + 7
    parts.append(f'<text x="14" y="{y+10}" class="head">ANGLE #50 — six comparator frames / 12 fps — ALSO UNADMITTED</text>')
    y += 24
    for j, n in enumerate(ANGLE_SAMPLES):
        item=all_frames['angle'][n]
        x=gap+j*tile_w
        parts.append(f'<image x="{x}" y="{y}" width="320" height="192" href="{item["url"]}" image-rendering="pixelated"/>')
        parts.append(f'<text x="{x}" y="{y+215}" class="small">Angle #{n:02d} / {n*1000/12:.0f}ms</text>')
        parts.append(f'<text x="{x}" y="{y+236}" class="meta">RGBA8 {escape(item["id"][:16])}…</text>')
    parts.append('</svg>')
    # These measurements are computed from verified native decoded pixels.
    # Reject unexpected evidence BEFORE creating an output file.
    if not (abs(percent[27]-7.84) < .01 and
            abs(percent[28]-4.48) < .01 and
            abs(percent[29]-42.85) < .01):
        raise ValueError('Archived native late-frame pixel changes do not match the pinned study')
    # Refuse to overwrite; prevent stale reviewer evidence being replaced.
    with dest.open('x', encoding='utf-8', newline='\n') as output:
        output.write('\n'.join(parts)+'\n')
    return dict(seat=32,angle=36,comparator=6,late_change_pct=[round(percent[n],2) for n in (27,28,29)],out=str(dest))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('verified_four_family_native_zip', type=Path)
    parser.add_argument('new_contact_sheet_svg', type=Path)
    args=parser.parse_args()
    print(build(args.verified_four_family_native_zip,args.new_contact_sheet_svg))
