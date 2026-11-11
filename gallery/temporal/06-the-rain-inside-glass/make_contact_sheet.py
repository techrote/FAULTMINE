#!/usr/bin/env python3
"""PRESENTATION ONLY: contact sheet from already rendered canonical CPU frames.
Requires Pillow; never substitutes thumbnail pixels for FAULTMINE output."""
import argparse, hashlib, json, pathlib
KEYS=(0,3,6,9,12,15,18,21,23)
def main():
    p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path)
    p.add_argument('--contact',type=pathlib.Path,required=True);args=p.parse_args()
    folder=args.directory
    audit=json.loads((folder/'native-cli-replay-audit.json').read_text(encoding='utf8'))
    assert audit['complete'] is True and audit['kind']=='authored-native-cli-not-fm012'
    assert (audit['frame_begin'],audit['frame_end_exclusive'])==(0,24)
    assert [r['frame'] for r in audit['frames']]==list(range(24))
    assert audit['out_of_order_replay']==[23,0,12,5,18,9]
    from PIL import Image, ImageDraw
    images=[]
    for n,record in enumerate(audit['frames']):
        f=folder/record['file']
        assert f.resolve().parent==folder.resolve(),'unsafe frame path'
        assert hashlib.sha256(f.read_bytes()).hexdigest()==record['encoded_png_sha256'],f.name
        im=Image.open(f);im.load()
        assert im.size==(256,176),f.name
        images.append(im.convert('RGBA'))
    assert args.contact.name.endswith('-PRESENTATION.png')
    if args.contact.exists():raise FileExistsError(args.contact)
    sheet=Image.new('RGB',(3*512,3*384),(7,11,23))
    draw=ImageDraw.Draw(sheet)
    for slot,frame in enumerate(KEYS):
        x=(slot%3)*512;y=(slot//3)*384
        sheet.paste(images[frame].resize((512,352),Image.Resampling.NEAREST),(x,y))
        draw.text((x+12,y+355),f'CANONICAL CPU FRAME {frame:02d} / PRESENTATION ONLY',fill=(193,235,219))
    sheet.save(args.contact)
    print('PASS: all 24 transport hashes/dimensions; contact sheet from nine actual native PNGs')
    print('NOT PROVEN: decoded image identity, FM-012 UI export/cancel, Debug/Release equivalence')
if __name__=='__main__':main()
