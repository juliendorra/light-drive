#!/usr/bin/env python3
"""Verify read-only import of the old LIGHTDRV.SAV score journal."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import zlib
from chgame_env import chgame_root
ROOT=Path(__file__).resolve().parents[1]
SDK=chgame_root() / 'platform/board/arduino/CHGame/libraries/CHSd'
sys.path.insert(0,str(SDK/'tools'))
from fatimg import build_image
with tempfile.TemporaryDirectory(prefix='lightdrive-import-') as temp:
    out=Path(temp)
    (out/'CHGame.h').write_text('#pragma once\n#include <stdint.h>\nuint8_t *gfx_chunkScratch();\nvoid gfx_wait();\n')
    (out/'test.cpp').write_text(r'''
#include "CardScore.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
alignas(4) uint8_t scratch[1024];
uint8_t *gfx_chunkScratch(){return scratch;}void gfx_wait(){}
namespace sd {
bool init(){return true;}
bool read(uint32_t lba,uint8_t *b){FILE *f=fopen(getenv("CHSD_CARD"),"rb");if(!f)return false;bool ok=!fseek(f,uint64_t(lba)*512,SEEK_SET)&&fread(b,1,512,f)==512;fclose(f);return ok;}
}
int main(){uint32_t best=0;bool ok=cardscore::load(best);printf("%u %u\n",ok,best);}
''')
    exe=out/'test'
    subprocess.run(['clang++','-std=c++17','-DCHSIM','-Wno-unknown-pragmas','-fsanitize=undefined,bounds','-fno-sanitize-recover=all',
        '-I'+str(out),'-I'+str(ROOT),'-I'+str(SDK/'src'),str(out/'test.cpp'),str(ROOT/'CardScore.cpp'),str(SDK/'src/Fat.cpp'),'-o',str(exe)],check=True)
    def score(generation,best):
        b=bytearray(512);struct.pack_into('<IIIHH',b,0,0x3244474c,generation,best,17,9);struct.pack_into('<I',b,20,zlib.crc32(b[:20]));return b
    for fs in ('fat16','fat32'):
        for variant in ('both','torn','damaged'):
            a=score(1,12000);b=score(2,16000)
            if variant!='both':b[8]^=128
            if variant=='damaged':a[8]^=128
            image=out/'card.img';build_image(image,{'LIGHTDRV.SAV':a+b,'OTHER.TXT':b'unchanged'},fs=fs,spc=1,fragment={'LIGHTDRV.SAV':2})
            before=image.read_bytes();result=subprocess.check_output([str(exe)],env=dict(os.environ,CHSD_CARD=str(image)),text=True).strip()
            assert result=={'both':'1 16000','torn':'1 12000','damaged':'0 0'}[variant],result
            assert image.read_bytes()==before
print('PASS: old Light Drive scores import from fragmented FAT16/FAT32 journals; corrupt newest records recover and all files remain untouched')
