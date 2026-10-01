"""Inventory runtime PNG dumps and make an inspection-only contact sheet."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from PIL import Image, ImageDraw

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    records = []
    files = sorted(args.source.glob('*.png'))
    sheet = Image.new('RGB', (1200, max(1, (len(files)+9)//10)*144), (32, 36, 42))
    draw = ImageDraw.Draw(sheet)
    for index, path in enumerate(files):
        with Image.open(path) as source:
            source = source.convert('RGBA')
            key = hashlib.sha256(struct.pack('<II', *source.size)+source.tobytes('raw', 'BGRA')).hexdigest()
            if key != path.stem:
                raise ValueError(f'Runtime content identity mismatch: {path}')
            alpha = source.getchannel('A').getextrema()
            records.append(dict(index=index, key=key, width=source.width, height=source.height, alpha_range=alpha))
            preview = source.copy()
            preview.thumbnail((112, 110))
            x, y = index % 10 * 120, index // 10 * 144
            sheet.paste(preview, (x, y+30), preview)
            draw.text((x, y), f'{index}: {key[:8]}', fill='white')
            draw.text((x, y+14), f'{source.width}x{source.height}', fill='white')
    args.destination.mkdir(parents=True, exist_ok=True)
    (args.destination/'manifest.json').write_text(json.dumps(records, indent=2))
    sheet.save(args.destination/'contact-sheet.png')
    print(f'{len(records)} texture identities verified; catalog saved to {args.destination}')

if __name__ == '__main__':
    main()
