from pathlib import Path
import os
def chgame_root():
    root=Path(__file__).resolve().parents[1]
    for path in [Path(os.environ.get('CHGAME_ROOT',str(root/'CHGame'))),root/'.deps/CHGame']:
        if (path/'tools/chgame.py').is_file():return path
    raise SystemExit('Clone bateske/CHGame into .deps/CHGame or set CHGAME_ROOT.')
