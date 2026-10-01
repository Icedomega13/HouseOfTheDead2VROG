"""Summarize the most recent process run from the append-only graphics probe log."""
import argparse
import json
from pathlib import Path
import re


def summarize(text):
    runs = re.split(r'(?m)(?=^\[\d+\] probe_version=)', text)
    current = next((s for s in reversed(runs) if 'probe_version=' in s), '')
    if not current:
        raise ValueError('No process startup record in log')
    def lines(pattern):
        return [line for line in current.splitlines() if re.search(pattern, line)]
    formats = sorted(set(re.findall(r'DRAW .*?fvf=([0-9a-f]+)', current)))
    scene = lines(r' SCENE ')
    return {
        'startup': current.splitlines()[0],
        'stereo_config': lines(r' STEREO enabled='),
        'observed_vertex_formats': formats,
        'last_scene_counters': scene[-1] if scene else None,
        'last_presentation': (lines(r' PRESENT ') or [None])[-1],
        'captures': lines(r' CAPTURE '),
        'xr_events': lines(r' XR '),
        'errors': lines(r'restore error|unsupported layout|begin_hr=(?!00000000)|XR failure'),
        'limitations': 'Observed run only. Draw counters count original calls, not doubled stereo submissions. Successful desktop rendering does not prove headset submission or controller support.'
    }


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    report=summarize(args.log.read_text(encoding='utf-8',errors='replace'))
    payload=json.dumps(report,indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(payload,encoding='utf-8')
    print(payload)
