#!/usr/bin/env python3
"""Asset-scoped regression for the pinned ART-TEMP-005 Windows native review."""
import base64
import json
import re
import sys
import tempfile
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import review_native


def run(archive: Path) -> None:
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / 'review.html'
        review_native.build(archive, out)
        html = out.read_text(encoding='utf-8')
        match = re.search(r'const data=(\[.*?\]);let n=0,', html)
        assert match, 'Missing ordered embedded data'
        items = json.loads(match.group(1))
        assert len(items) == 32
        assert len({x['id'] for x in items}) == 32
        assert all(x['src'].startswith('data:image/png;base64,') for x in items)
        # Embedded PNGs are exact, original PNG bytes, not decoded/re-encoded proxies.
        with ZipFile(archive) as original:
            for i, row in enumerate(items):
                expected_name = f'/CANONICAL-REVIEW-EVIDENCE/seat-05-release/seat-count_{i:06d}.png'
                name = next(n for n in original.namelist() if n.endswith(expected_name))
                assert base64.b64decode(row['src'].split(',', 1)[1]) == original.read(name)
            names = [n for n in original.namelist() if review_native.PREFIX in n]
            data = {n: original.read(n) for n in names}
        def reject(changed, reason):
            candidate = Path(td) / 'bad.zip'
            with ZipFile(candidate, 'w', compression=ZIP_DEFLATED) as z:
                for n, b in data.items():
                    z.writestr(n, changed(n, b))
            try:
                review_native.build(candidate, out)
            except (ValueError, KeyError) as e:
                assert reason in str(e), (reason, str(e))
            else:
                raise AssertionError('Tampering must be rejected: ' + reason)
        reject(lambda n, b: b + b'!' if n.endswith('native-cli-replay-audit.json') else b, 'receipt changed')
        reject(lambda n, b: b[:-1] + bytes([b[-1] ^ 1]) if n.endswith('seat-count_000010.png') else b, 'frame 10')
        reject(lambda n, b: b if not n.endswith('seat-count_000011.png') else b'not-png', 'frame 11')
        print('PASS native provenance, 32 embedded original PNGs, receipt and frame-corruption rejection')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: python test_review_native.py NATIVE-EVALUATION.zip')
    run(Path(sys.argv[1]))
