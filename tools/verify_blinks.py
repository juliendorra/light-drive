#!/usr/bin/env python3
"""Check windshield shutters, visible instruments and mid-blink wake-ups."""
from pathlib import Path
import json,sys
from chgame_env import chgame_root
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));from verify_sim import state,tap,save_gif,labeled_sheet
sys.path.insert(0,str(chgame_root() / 'tools/chsim'))
from chsim import build
from chdrivelib import Driver,SimTransport,mask_of
from fbimage import to_image
from PIL import Image,ImageDraw
out=ROOT/'out/blink-review';out.mkdir(parents=True,exist_ok=True)
t=SimTransport(build(ROOT,['NIGHT_CAPTURE'],out=out/'blinks.exe'));d=Driver(t,'NIGHT')
def ink(dump,x,y):return (dump[y*64+x//2]>>((x&1)*4))&15
def edge(x):return 99-x*3//44 if x<=44 else 96+(x-44)*3//40 if x<=84 else 99
def trip_pixel(x,y):return 3<=y<10 and (7<=x<29 or 94<=x<124)
def black_view(dump):return all(ink(dump,x,y)==0 for x in range(128) for y in range(edge(x)) if not trip_pixel(x,y))
def visible_dashboard(dump):return any(dump[99*64:8192])
def pair(images,labels):
 im=Image.new('RGB',(768,408),(12,9,9));draw=ImageDraw.Draw(im)
 for i,(pic,label) in enumerate(zip(images,labels)):
  im.paste(pic,(384*i,0));draw.text((384*i+7,390),label,fill=(255,186,164))
 return im
try:
 d.handshake();d.cmd('L1');d.buttons(0)
 # Identical staged world/tick across phases lets us compare every pixel
 # with the open frame. The shutter changes only its curved view mask.
 d.cmd('X 1800 0');d.frames(1);reference=d.shot();checks=[];keyframes=[];labels=[]
 for phase in [0,46,47,48,49,66,67,68,69,70,71]:
  d.cmd(f'X 1800 {phase}');d.frames(1);s=state(d);dump=d.shot();amount=s[31]
  assert visible_dashboard(dump) and dump[99*64:8192]==reference[99*64:8192]
  assert all(ink(dump,x,y)==ink(reference,x,y) for y in range(3,10) for x in range(128) if trip_pixel(x,y)),'time and distance must remain unchanged through every shutter phase'
  if s[30]:assert black_view(dump),'shut view must suppress road, neon, guide and overlays'
  else:
   cy=59 # staged tick 1201: integer wave offset is zero
   for x in range(128):
    curve=(x-64)*(x-64)//256;opening=15*(256-amount)//256
    top=cy-opening+curve*amount//256
    bottom=max(top,cy+opening-curve*amount//256)
    for y in range(128):
     masked=amount and y<edge(x) and not trip_pixel(x,y) and (y<top or y>=bottom)
     assert ink(dump,x,y)==(0 if masked else ink(reference,x,y)),(phase,amount,x,y)
  checks.append({'phase':s[29],'shutter':amount,'closed':bool(s[30]),'dashboard_identical':True,'time_and_km_identical':True})
  if phase in [0,47,48,49,68,70]:keyframes.append(to_image(dump,3));labels.append(f'SHUTTER {amount}/256')
 d.cmd('X 1800 60');d.frames(1);assert black_view(d.shot()) and state(d)[30]
 before=state(d);cockpit=d.shot()[99*64:8192];tap(d,'START');assert state(d)[0]==3 and not black_view(d.shot())
 assert d.shot()[99*64:8192]==cockpit,'pause must preserve the same instruments'
 frozen=state(d);pixels=d.shot();d.frames(180);assert state(d)==frozen and d.shot()==pixels
 tap(d,'START');assert state(d)[0]==2 and state(d)[30] and black_view(d.shot())
 assert state(d)[2]>before[2],'resuming advances the drive, even with closed eyes'
 # Both start inside a full blink. Actual world-distance contact/pickup
 # wakes the driver before the blink would otherwise end.
 clips=[];rescues=[]
 for fixture,event,name in [('U 0 1 21 9500 0 1800 55',1,'NEON ARROW'),('W 0 3 550 9500 80 1800 55',2,'DEER IMPACT')]:
  d.buttons(0);d.cmd(fixture);d.frames(1);assert black_view(d.shot()) and state(d)[30] and visible_dashboard(d.shot())
  frames=[to_image(d.shot(),3)];trace=[state(d)];wasBlack=True;rescued=False
  for f in range(179):
   s=state(d);d.buttons(mask_of('RIGHT') if s[3]<430 else mask_of('LEFT') if s[3]>600 else 0)
   d.frames(1);s=state(d);frames.append(to_image(d.shot(),3));trace.append(s)
   if s[16]&event and not rescued:
    assert wasBlack and not s[30] and not black_view(d.shot()),'pickup/impact must immediately interrupt the shutter'
    assert s[28]==0 and s[29]==0
    if event==2:assert s[4]==0 and s[15]==1
    rescued=True
   wasBlack=bool(s[30])
  assert rescued and state(d)[0]==2
  rescues.append({'kind':name,'rescue_frame':next(i+1 for i,s in enumerate(trace) if s[16]&event),'sleep_after':next(s[4] for s in trace if s[16]&event)})
  clips.append(frames)
 if '--capture' in sys.argv:
  d.buttons(0);d.cmd('X 1800 35');shutter_frames=[]
  for f in range(72):
   d.frames(1);shutter_frames.append(to_image(d.shot(),4))
  save_gif(shutter_frames,ROOT/'docs/windshield-shutter.gif',1000/60)
  labeled_sheet(keyframes,labels,3).save(ROOT/'docs/windshield-shutter.png')
  d.buttons(0);d.cmd('X 0');frames=[];trace=[];previous=0;braking=False
  for f in range(800):
   s=state(d);velocity=(s[3]-previous)/3 if f else 0;previous=s[3]
   target=s[3]+velocity*10
   if s[5]>82:braking=True
   if s[5]<72:braking=False
   keys=(mask_of('B') if braking else 0)|(mask_of('RIGHT') if target<-30 else mask_of('LEFT') if target>30 else 0)
   d.buttons(keys);d.frames(3);s=state(d);dump=d.shot();trace.append(s)
   assert s[0]==2 and s[4]==10000 and s[2]>420*256,'fatigue cannot end the drive'
   assert visible_dashboard(dump),'instruments must remain visible throughout the blink'
   if s[30]:assert black_view(dump),'full closure must cover the windshield, including neon'
   frames.append(to_image(dump,3))
  save_gif(frames,ROOT/'docs/sleep-blinks.gif',50)
  comparison=[pair([frames[i],frames[560+i]],['FIRST 12 SECONDS','AFTER 28 SECONDS']) for i in range(240)]
  save_gif(comparison,ROOT/'docs/blink-cadence.gif',50)
  waking=[pair([clip[i] for clip in clips],['NEON WAKES YOU','AN ANIMAL HIT WAKES YOU']) for i in range(180)]
  save_gif(waking,ROOT/'docs/blink-wakeups.gif',1000/60)
  views=[];labels=[]
  for clip,name in zip(clips,['NEON','DEER']):
   for n in [0,10,22,75]:views.append(clip[n]);labels.append(f'{name} / {n/60:.2f} S')
  labeled_sheet(views,labels,4).save(ROOT/'docs/blink-wakeups.png')
  starts=[i*3+3 for i,s in enumerate(trace) if s[30] and (i==0 or not trace[i-1][30])]
  (out/'progression.json').write_text(json.dumps(trace)+'\n')
  report={'staged_start':True,'controls':'Scripted lane keeping and braking solely to review blinking; ordinary game updates continue.','playback_speed':1,'progression_seconds':40,'start_frames_sampled_every_3_ticks':starts,'rescue_cases':rescues,'interval_seconds':[5,1.2],'closed_seconds':.3,'shutter':{'closing_seconds':3/60,'opening_seconds':4/60,'scope':'Curved windshield only; dashboard, time and kilometres always visible.','pixel_checks':checks},'ramp_seconds':30,'sleep_game_over':False,'minimum_aperture_unchanged':True,'paused_menu_visible':True}
  (ROOT/'docs/sleep-blinks.json').write_text(json.dumps(report,indent=2)+'\n')
 assert not t.bugs,t.bugs
finally:t.close()
print('PASS: pixel-exact curved windshield shutter, unchanged visible dashboard, frozen pause, live controls, neon/deer wake-up; no torn frames')
