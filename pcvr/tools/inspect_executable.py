"""Read-only PE32 graphics/input reconnaissance; no third-party dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def inspect(path):
    data = path.read_bytes()
    def u16(p): return struct.unpack_from('<H', data, p)[0]
    def u32(p): return struct.unpack_from('<I', data, p)[0]
    if data[:2] != b'MZ': raise ValueError('Not a DOS/PE executable')
    pe = u32(0x3c)
    if data[pe:pe+4] != b'PE\0\0': raise ValueError('Missing PE signature')
    opt = pe + 24
    if u16(opt) != 0x10b: raise ValueError('This probe expects PE32')
    base = u32(opt + 28)
    sections = []
    for i in range(u16(pe + 6)):
        p = opt + u16(pe + 20) + i * 40
        sections.append(dict(name=data[p:p+8].rstrip(b'\0').decode('ascii', 'replace'),
            virtual_size=u32(p+8), rva=u32(p+12), raw_size=u32(p+16),
            offset=u32(p+20), flags=u32(p+36)))
    def offset(rva):
        if rva < u32(opt+60) and rva < len(data): return rva
        for s in sections:
            d = rva - s['rva']
            if 0 <= d < s['raw_size'] and s['offset'] + d < len(data):
                return s['offset'] + d
        raise ValueError(f'RVA has no file-backed bytes: {rva:#x}')
    def string(rva):
        p = offset(rva)
        end = data.find(b'\0', p, min(p+4096, len(data)))
        if end < 0: raise ValueError('Unterminated string')
        return data[p:end].decode('ascii', 'replace')
    imports = []
    imp = u32(opt+104)
    if imp:
        for i in range(4096):
            p = offset(imp+i*20)
            oft, _, _, name, ft = struct.unpack_from('<5I', data, p)
            if not any(data[p:p+20]): break
            symbols = []
            for n in range(65536):
                value = u32(offset((oft or ft)+4*n))
                if not value: break
                symbols.append(dict(name=f'ordinal:{value & 0xffff}' if value & 0x80000000
                    else string(value+2), iat_va=f'0x{base+ft+4*n:08x}'))
            else: raise ValueError('Unterminated import thunk table')
            imports.append(dict(dll=string(name), symbols=symbols))
        else: raise ValueError('Unterminated import directory')
    candidates = []
    for m in re.finditer(rb'[\x20-\x7e]{5,}', data):
        value = m.group().decode('ascii')
        if not re.search(r'direct|ddraw|d3d|camera|\.cam|cp_st|st1ev|\.dll|\.pdb', value, re.I): continue
        s = next((s for s in sections if s['offset'] <= m.start() < s['offset']+s['raw_size']), None)
        va = base+s['rva']+m.start()-s['offset'] if s else None
        refs = []
        if va:
            needle = struct.pack('<I', va)
            for code in sections:
                if not code['flags'] & 0x20000000: continue
                chunk = data[code['offset']:code['offset']+code['raw_size']]
                start = 0
                while (hit := chunk.find(needle, start)) >= 0:
                    refs.append(f"0x{base+code['rva']+hit:08x}")
                    start = hit+1
        candidates.append(dict(text=value, file_offset=m.start(),
            va=f'0x{va:08x}' if va else None, possible_absolute_references=refs))
    return dict(file=str(path.resolve()), sha256=hashlib.sha256(data).hexdigest(),
        bytes=len(data), machine=f'0x{u16(pe+4):04x}', image_base=f'0x{base:08x}',
        entry_va=f'0x{base+u32(opt+16):08x}', sections=sections, imports=imports,
        candidate_strings=candidates,
        limitations='Static PE32 inspection. Byte matches are not decoded instructions or verified function addresses. COM methods, runtime loads, camera ownership and stereo feasibility require a runtime trace.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inspect(args.executable)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('sha256', 'machine', 'entry_va')}))
    for item in result['imports']:
        print(item['dll'] + ': ' + ', '.join(s['name'] for s in item['symbols']))
