"""Compare paired diagnostic eye crops, including zero-separation controls."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops


def compare(root):
    result={}
    for mode in ('stereo','zero'):
        folder=root/mode
        if not folder.is_dir(): raise ValueError(f'Missing capture folder: {folder}')
        result[mode]={}
        for path in sorted(folder.glob('frame-*.bmp')):
            with Image.open(path) as source:
                if source.size != (640,480): raise ValueError(f'Unexpected frame size: {path}')
                im=source.convert('RGB')
            left=im.crop((0,120,320,360));right=im.crop((320,120,640,360))
            diff=ImageChops.difference(left,right).tobytes()
            changed=sum(bool(diff[i] or diff[i+1] or diff[i+2]) for i in range(0,len(diff),3))
            result[mode][path.name]={'different_pixels':changed,'total_pixels':76800}
    for frame in ('frame-001260.bmp','frame-001500.bmp','frame-001800.bmp'):
        if frame not in result['zero'] or frame not in result['stereo']:
            raise ValueError(f'Missing paired world-scene capture: {frame}')
        if result['zero'][frame]['different_pixels']>76:
            raise ValueError(f'Zero-separation eye difference exceeds 0.1%: {frame}')
        if result['stereo'][frame]['different_pixels']<768:
            raise ValueError(f'Separated eye difference below 1%: {frame}')
    result['validation']='PASS: near-identical zero-separation eyes and distinct separated world views. This does not validate physical scale or headset comfort.'
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures',type=Path)
    args=parser.parse_args()
    report=compare(args.captures)
    payload=json.dumps(report,indent=2)
    (args.captures/'comparison.json').write_text(payload,encoding='utf-8')
    print(payload)
