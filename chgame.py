#!/usr/bin/env python3
"""Build, test and package Light Drive with CHSave."""
from pathlib import Path
import subprocess,sys
ROOT=Path(__file__).resolve().parent
raise SystemExit(subprocess.call([sys.executable,str(ROOT/'tools/sd_saves.py'),'run','--sketch',str(ROOT),*(sys.argv[1:] or ['build'])]))
