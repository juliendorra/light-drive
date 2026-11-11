#!/usr/bin/env python3
"""Read/write/restart the real game with its namespaced journal."""
from pathlib import Path
import os,struct,sys,zlib
G=Path(__file__).resolve().parents[1];R=G
sys.path[:0]=[str(G/'tools'),str(R/'CHGame/tools/chsim'),str(R/'chgame-sd-saves/tools'),str(R/'CHGame/platform/board/arduino/CHGame/libraries/CHSd/tools')]
import prepare
from sd_saves import overlay
from fatimg import build_image,FatVolume
from chsim import build
from chdrivelib import Driver,SimTransport
O=G/'out/card-check';O.mkdir(parents=True,exist_ok=True)
os.environ['CHSIM_CHGAME']=str(overlay()/'CHGame/src')
exe=build(G);ID='com.juliendorra.lightdrive';NAME=prepare.save_path(ID)
page=bytearray(512);struct.pack_into('<IIHBBI',page,0,prepare.RECORD_TAG,8,8,1,0,0)
struct.pack_into('<IBBBB',page,16,16000,2,1,0,0);struct.pack_into('<I',page,508,zlib.crc32(page[:508]))
raw=prepare.descriptor(ID,1)+page+b'\xff'*512
image=O/'card.img';old=b'ignored old distance journal'
build_image(image,{NAME:raw,'LIGHTDRV.SAV':old,'OTHER.TXT':b'unchanged'},fs='fat16',spc=1,fragment={NAME:3})
os.environ['CHGAME_SAVE_CARD']=str(image)
for restart in range(2):
 t=SimTransport(exe);d=Driver(t,'NIGHT')
 try:
  d.handshake();d.frames(1);state=list(map(int,d.query('H','NIGHTSTATE').split()[1:]))
  assert state[26]==16000 and state[6:8]==[2,1] and state[18]==0,state
  assert d.query('G','CARDSCORE').split()[1:]==['1','16000']
 finally:assert t.close()==0 and not t.bugs,t.bugs
with FatVolume(image) as volume:
 after=volume.read_file(NAME);prepare.validate(after,ID)
 assert after==raw and volume.read_file('LIGHTDRV.SAV')==old and volume.read_file('OTHER.TXT')==b'unchanged'
print('PASS: hashed filename, fragmented SD save, score and settings across restarts, unchanged-write suppression; old journal ignored')
