#!/usr/bin/env python3
"""Run actual release firmware, measuring a steady window in the native emulator."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--emulator',type=Path,default=ROOT.parent/'.cache/chgame-emulator-review/chg_headless_fixed')
a=p.parse_args()
out=ROOT/'out/emulator';out.mkdir(parents=True,exist_ok=True)
binary=ROOT/'build/release/LightDrive.ino.bin'
counts=[]
for seconds in [8,12]:
    run=subprocess.run([str(a.emulator.resolve()),str(binary),str(seconds),str(out/f'{seconds}.ppm'),
        '--press','a@1.0:0.15','--wav',str(out/f'{seconds}.wav'),'--piezo'],capture_output=True,text=True,check=True)
    log=run.stdout+run.stderr;(out/f'{seconds}.log').write_text(log)
    assert 'FAULT' not in log and 'display on' in log,log
    counts.append(int(re.search(r'frames written (\d+)',log)[1]))
fps=(counts[1]-counts[0])/4
# CHGame's accumulator can catch up three frames after a long render, so
# a four-second LCD-start window may include that backlog plus rounding.
# The two round cockpit gauges and shared high-speed perspective keep the
# opening close to 60 Hz, with scene-dependent frame costs. This is a
# regression floor, not a claim that every four-second window is 60 FPS.
assert 57<=fps<=61,fps
report={'firmware_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
    'steady_opening_fps':fps,'sample_seconds':[8,12],'frame_counts':counts,'cpu_faults':0,
    'limits':['Actual RISC-V release firmware in the native emulator.',
              'Opening gameplay window; LCD-start counts include up to three catch-up frames.',
              'No physical device performance measurement.']}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS: actual release firmware, {fps:.2f} fps in opening window, no CPU faults; piezo WAV recorded')
