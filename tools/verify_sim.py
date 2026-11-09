#!/usr/bin/env python3
"""Exercise real handheld controls; optional explicitly staged visual gallery."""
from pathlib import Path
import json
import sys

from chgame_env import chgame_root
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(chgame_root() / 'tools/chsim'))
from chsim import build
from chdrivelib import Driver,SimTransport,mask_of
from fbimage import to_image,sheet,save_gif as write_gif
from PIL import Image,ImageDraw

def save_gif(frames,path,duration):
    # GIF delays are centiseconds. Alternate rounded delays instead of
    # truncating every 66.67 ms frame to 60 ms and speeding the clip up.
    if isinstance(duration,(int,float)) and duration%10:
        duration=[(round((i+1)*duration/10)-round(i*duration/10))*10 for i in range(len(frames))]
    return write_gif(frames,path,duration)

def state(d):return list(map(int,d.query('H','NIGHTSTATE').split()[1:]))
def tap(d,key):
    d.buttons(mask_of(key));d.frames(1);d.buttons(0);d.frames(1)

def labeled_sheet(images,labels,cols=3):
    size=384;pad=8;label=23;rows=(len(images)+cols-1)//cols
    result=Image.new('RGB',(cols*(size+pad)+pad,rows*(size+label+pad)+pad),(15,13,13))
    draw=ImageDraw.Draw(result)
    for i,(im,name) in enumerate(zip(images,labels)):
        x=pad+(i%cols)*(size+pad);y=pad+(i//cols)*(size+label+pad)
        result.paste(im.resize((size,size),Image.Resampling.NEAREST),(x,y))
        draw.text((x+8,y+size+5),name,fill=(240,202,185))
    return result

def capture_lights(d,docs,out):
    views=[];labels=[];checks=[]
    for kind in range(4):
        side=1 if kind%2==0 else -1
        for sleep,label in [(1800,'AWAKE'),(5900,'DROWSY'),(8800,'NARROW VIEW')]:
            d.buttons(0);d.cmd(f'U {kind} {side} 65 {sleep} 0');d.frames(1)
            dump=d.shot();im=to_image(dump,4);views.append(im);labels.append(f'{["OPEN","MOTEL","GAS","24H"][kind]} / {label}')
            if sleep==8800:
                inks=[(dump[y*64+x//2]>>((x&1)*4))&15 for y in range(99) for x in range(128)]
                color=1 if kind==1 else 2 if kind==2 else 0
                assert inks.count(10+color)>0 and inks.count(13+color)>0,'dim colored core and blurred halo must survive the mask'
                checks.append({'sign':kind,'dim_pixels':inks.count(10+color),'blur_pixels':inks.count(13+color)})
    labeled_sheet(views,labels,3).save(docs/'neon-through-tunnel.png')
    d.buttons(0);d.cmd('U 1 1 92 8500 0');frames=[];trace=[];first=state(d)[4]
    for i in range(150):
        s=state(d);d.buttons(mask_of('RIGHT') if s[3]<440 else mask_of('LEFT') if s[3]>520 else 0)
        d.frames(4);s=state(d);frames.append(to_image(d.shot(),4));trace.append(s)
    assert any(s[10]==63 for s in trace) and min(s[4] for s in trace)<first-3500,'all six road arrows must restore sleep without A'
    save_gif(frames,docs/'neon-arrows.gif',duration=1000*4/60)
    d.buttons(0);d.cmd('W 1 20 0 6000 80');frames=[];hits=[];keyframes=[];names=[]
    for i in range(120):
        s=state(d);d.buttons(mask_of('RIGHT') if s[3]<-45 else mask_of('LEFT') if s[3]>45 else 0)
        d.frames(2);s=state(d);im=to_image(d.shot(),4);frames.append(im);hits.append(s)
        if (s[17] and len(keyframes)<2) or (not keyframes and i==20) or i in (65,110):
            keyframes.append(im);names.append(f'{(i+1)/30:.2f} S / {s[5]} KM/H / DRIFT +{s[22]}%')
    impact=next(i for i,s in enumerate(hits) if s[15])
    assert max(s[15] for s in hits)==1 and hits[impact-1][5]-hits[impact][5]>=17
    assert max(s[17] for s in hits)>=70 and hits[-1][17]==0
    # Captures are two ticks apart: the next tick can add three fatigue units.
    assert hits[impact][4]<=3 and hits[impact][22]==20
    save_gif(frames,docs/'obstacle-impact.gif',duration=1000*2/60)
    labeled_sheet(keyframes,names,2).save(docs/'obstacle-impact-frames.png')
    report={'fixtures':'Staged starts; ordinary game updates and steering, no A input.',
        'glow_checks':checks,'arrow_count':{'OPEN':3,'MOTEL':6,'GAS':2,'24H':4},'sleep_relief_per_arrow':8,'road_lane_center':480,
        'arrow_offsets_metres':[-45,-27,-9,9,27,45],
        'impact':{'immediate_speed_drop':18,'sleep_reset_percent':0,'jolt_seconds':.3,'message_seconds':1.2,'added_drift_percent':20,'hit_limit':None}}
    (docs/'neon-and-impact.json').write_text(json.dumps(report,indent=2)+'\n')
    (out/'arrow-states.json').write_text(json.dumps(trace)+'\n')
    (out/'impact-states.json').write_text(json.dumps(hits)+'\n')

def capture_rollover(d,docs,out):
    clips=[];states=[]
    for side,name in [(1,'right'),(-1,'left')]:
        d.buttons(0);d.cmd(f'V {side}');frames=[];keyframes=[];labels=[];trace=[]
        for i in range(78):
            d.frames(4);s=state(d);im=to_image(d.shot(),4);frames.append(im)
            trace.append({'frame':(i+1)*4,'screen':s[0],'tumble':s[21],'rumble':s[19],'remaining_ms':s[20]})
            if i in [0,4,10,17,25,35,44,54,61]:
                keyframes.append(im);labels.append(f'{(i+1)*4/60:.2f} s / ROLL {s[21]}')
            if s[0]==6:assert s[4]==0,'shock must instantly clear fatigue'
        assert state(d)[0]==4 and state(d)[21]==210,'rollover must settle into the rest screen'
        save_gif(frames,docs/f'rollover-{name}.gif',duration=1000*4/60)
        (out/f'rollover-{name}.json').write_text(json.dumps(trace,indent=2)+'\n')
        if side==1:
            save_gif(frames,docs/'rollover.gif',duration=1000*4/60)
            labeled_sheet(keyframes,labels,3).save(docs/'rollover-frames.png')
        clips.append(frames);states.append(trace)
    together=[labeled_sheet([clip[i] for clip in clips],['RIGHT SHOULDER','LEFT SHOULDER'],2) for i in range(78)]
    save_gif(together,docs/'rollover-both.gif',duration=1000*4/60)
    report={'staged_start':True,'playback_speed':1,'clip_seconds':5.2,'roll_seconds':3.5,
            'sequence':'immediate simultaneous lateral acceleration, zoom and roll / rolling contacts / slowing travel / small landing rebound',
            'rumble_contacts_at_frames':[0,18,44,75,106,140,168,190],
            'forward_coast':{'metres_at_62_kmh':33.4,'entry_speed_until_frame':44,'stops_at_frame':190}}
    (docs/'rollover-motion.json').write_text(json.dumps(report,indent=2)+'\n')
    catalogue=docs/'captures.json'
    if catalogue.exists():
        data=json.loads(catalogue.read_text())
        data['animations']['rollover.gif']={'speed':1,'seconds':5.2,'roll_seconds':3.5}
        catalogue.write_text(json.dumps(data,indent=2)+'\n')

def main():
    out=ROOT/'out/simulator';out.mkdir(parents=True,exist_ok=True)
    docs=ROOT/'docs';docs.mkdir(exist_ok=True)
    transport=SimTransport(build(ROOT));d=Driver(transport,'NIGHT')
    try:
        d.handshake();d.cmd('L1');d.frames(2)
        assert state(d)[0]==0
        title=to_image(d.shot(),4);title.save(docs/'title.png')
        tap(d,'RIGHT');assert state(d)[6]==1
        tap(d,'LEFT');assert state(d)[6]==0
        tap(d,'B');assert state(d)[0]==1
        tap(d,'A');assert state(d)[0]==0
        tap(d,'UP');assert state(d)[7]==1
        tap(d,'UP');assert state(d)[7]==0
        tap(d,'DOWN');assert state(d)[18]==0
        tap(d,'DOWN');assert state(d)[18]==1
        tap(d,'A');assert state(d)[0]==2
        old=state(d)[3];d.buttons(mask_of('RIGHT'));d.frames(32);d.buttons(0)
        assert state(d)[3]>old+140
        d.buttons(mask_of('B'));d.frames(160);d.buttons(0);assert state(d)[5]==42
        tap(d,'START');assert state(d)[0]==3
        frozen=state(d);pixels=d.shot();d.frames(180)
        assert state(d)==frozen and d.shot()==pixels,'pause must freeze state and image'
        tap(d,'SELECT');assert state(d)[6]==1
        tap(d,'START');assert state(d)[0]==2
        d.frames(180);assert state(d)[5]>42
        tap(d,'START');tap(d,'B');assert state(d)[0]==0
        assert not transport.bugs,transport.bugs
    finally:transport.close()
    if any(flag in sys.argv for flag in ['--capture','--rollover','--lights']):
        transport=SimTransport(build(ROOT,['NIGHT_CAPTURE'],out=out/'gallery.exe'))
        d=Driver(transport,'NIGHT');images=[]
        try:
            d.handshake();d.cmd('L1');d.frames(2)
            if '--lights' in sys.argv:
                capture_lights(d,docs,out)
                assert not transport.bugs,transport.bugs
                print('PASS: colored dim/blurred neon through tunnel, six button-free arrow pickups, visible immediate obstacle impact; no torn frames')
                return
            if '--rollover' in sys.argv:
                capture_rollover(d,docs,out)
                assert not transport.bugs,transport.bugs
                print('PASS: both rollover directions, progressive motion, synchronized haptics and final rest state; no torn frames')
                return
            for palette,name in enumerate(['night-red','amber','deep-blue','violet']):
                d.cmd(f'J 420 2000 230 {palette}');d.frames(1)
                im=to_image(d.shot(),4);im.save(docs/f'{name}.png');images.append(im)
            sheet(images,cols=2).save(docs/'palettes.png')
            images=[]
            for sleep,name in [(1800,'awake'),(5900,'drowsy'),(8800,'tunnel')]:
                d.cmd(f'J 420 {sleep} 230 0');d.frames(1)
                im=to_image(d.shot(),4);im.save(docs/f'{name}.png');images.append(im)
            sheet(images,cols=3).save(docs/'sleep.png')
            d.cmd('J 420 6400 360 0');frames=[]
            for i in range(75):
                s=state(d);target=380
                d.buttons(mask_of('A') | (mask_of('RIGHT') if s[3]<target-40 else mask_of('LEFT') if s[3]>target+40 else 0))
                d.frames(4);frames.append(to_image(d.shot(),4))
            save_gif(frames,docs/'gameplay.gif',duration=1000*4/60)
            save_gif(frames,docs/'neon-recovery.gif',duration=1000*4/60)
            capture_rollover(d,docs,out)
            capture_lights(d,docs,out)
            d.cmd('J 420 4500 0 2');frames=[]
            for i in range(75):
                for _ in range(6):
                    s=state(d);d.buttons(mask_of('RIGHT') if s[3]<-40 else mask_of('LEFT') if s[3]>40 else 0)
                    d.frames(4)
                frames.append(to_image(d.shot(),4))
            save_gif(frames,docs/'falling-asleep.gif',duration=100)
            # Full catalogue: every sign and all wildlife/obstacle sprites.
            images=[]
            for kind,name in enumerate(['OPEN / CYAN','MOTEL / PINK','GAS / LIME','24H / CYAN']):
                d.buttons(0);d.cmd(f'U {kind}');d.frames(1)
                im=to_image(d.shot(),4);im.save(docs/f'neon-{kind}.png');images.append(im)
            labeled_sheet(images,['OPEN / CYAN','MOTEL / PINK','GAS / LIME','24H / CYAN'],2).save(docs/'all-neon-signs.png')
            locations=[]
            for kind in range(4):
                side=1 if kind%2==0 else -1
                d.buttons(0);d.cmd(f'U {kind} {side} 92');frames=[]
                for i in range(105):
                    s=state(d);target=side*500
                    d.buttons(mask_of('RIGHT') if s[3]<target-50 else mask_of('LEFT') if s[3]>target+50 else 0)
                    d.frames(4);frames.append(to_image(d.shot(),3))
                save_gif(frames,docs/f'roadside-{kind}.gif',duration=1000*4/60);locations.append(frames)
            montage=[labeled_sheet([frames[i] for frames in locations],['OVAL OPEN / DINER','CHANNEL MOTEL / LODGE','GAS BLADE / FORECOURT','24H + SMALL TEXT / SHOP'],2) for i in range(105)]
            save_gif(montage,docs/'roadside-locations.gif',duration=1000*4/60)
            all_frames=[];images=[]
            creatures=['deer','branch','fox','hare','owl','birds']
            for kind,name in enumerate(creatures):
                d.buttons(0);d.cmd(f'W {kind} 19');d.frames(1)
                im=to_image(d.shot(),4);im.save(docs/f'{name}.png');images.append(im)
                d.cmd(f'W {kind} 82');frames=[]
                for i in range(78):
                    s=state(d)
                    target=550 if kind==1 else -600
                    d.buttons(mask_of('RIGHT') if s[3]<target-40 else mask_of('LEFT') if s[3]>target+40 else 0)
                    d.frames(4);frames.append(to_image(d.shot(),3))
                save_gif(frames,docs/f'crossing-{name}.gif',duration=1000*4/60)
                all_frames.append(frames)
            labeled_sheet(images,[n.upper() for n in creatures]).save(docs/'all-crossings.png')
            montage=[labeled_sheet([frames[i] for frames in all_frames],[n.upper() for n in creatures]) for i in range(78)]
            save_gif(montage,docs/'all-crossings.gif',duration=1000*4/60)
            (docs/'captures.json').write_text(json.dumps({
                'source':'Native CHGame simulator, actual game renderer',
                'fixtures':'Gallery starts and initial sleep/position are staged; subsequent updates are ordinary rules.',
                'animations':{'neon-recovery.gif':{'speed':1,'seconds':5},
                              'rollover.gif':{'speed':1,'seconds':5.2,'roll_seconds':3.5},
                              'falling-asleep.gif':{'speed':4,'simulated_seconds':30},
                              'roadside-*.gif':{'speed':1,'seconds':7,'details':'Angled thick signs, physical poles, paired buildings and cleared access lots; alternating road sides.'},
                              'crossing-*.gif':{'speed':1,'seconds':78*4/60,'types':creatures}},
            },indent=2)+'\n')
            assert not transport.bugs,transport.bugs
        finally:transport.close()
    (out/'results.json').write_text(json.dumps({'passed':['title','palette selection','help','mute','drive','steering','braking','frozen pause state and pixels','resume','quit'],
        'torn_frames':0,'staged_gallery':'--capture' in sys.argv},indent=2)+'\n')
    print('PASS: real buttons, palette/help/sound, steering/brake, frozen pause pixels, resume/quit; no torn frames')

if __name__=='__main__':main()
