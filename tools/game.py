"""Shared 60 Hz driving rules and real-button simulator validation."""
import json
import subprocess
import sys

TESTS={"rules":{"sources":["tools/tests/rules.cpp","Game.cpp","Rollover.cpp"],"args":"none"},
       "blinks":{"sources":["tools/tests/blinks.cpp","Game.cpp"],"args":"none"},
       "scenery":{"sources":["tools/tests/scenery.cpp","Scenery.cpp","Game.cpp"],"args":"none"},
       "rollover":{"sources":["tools/tests/rollover.cpp","Game.cpp","Rollover.cpp"],"args":"none"},
       "neon":{"sources":["tools/tests/neon.cpp","Neon.cpp"],"args":"none"},
       "avoidance":{"sources":["tools/tests/avoidance.cpp","Game.cpp"],"args":"none"},
       "human":{"sources":["tools/tests/human.cpp","Game.cpp","Scenery.cpp"],"run":False}}
EXTRA_TEST_SCRIPTS=["tools/verify_art.py","tools/verify_sim.py","tools/verify_blinks.py","tools/verify_rumble.py","tools/verify_card.py"]

def before_tests(ctx):
    subprocess.run([sys.executable,str(ctx.game/'tools/build_art.py'),'--check'],check=True)

def after_tests(ctx,exes):
    report=json.loads(subprocess.check_output([str(exes['human']),'128'],text=True))
    report['model']={
        'time':'Every game tick is 1/60 second; execution is accelerated in wall time.',
        'observations':'Noisy, intermittent road/trajectory estimates; visible hazards, dim peripheral neon and its painted arrow lane; dead reckoning when the stripe is hidden.',
        'attention':'Brief random lapses, 88% initial willingness to follow the road arrows (97% when very tired), attention switches to avoiding obstacles.',
        'controls':'Discrete D-pad decisions with delayed observations and imperfect anticipation. Speed checks every 3 seconds (4 for slower reactions); brake down from 85 toward 74. Cautious braking persists after an encounter.',
        'near_miss':'Obstacle clearance below 300 lateral units beyond its collision width, or a separate excursion beyond 850 toward the 1024-unit shoulder.',
        'limits':'An explicit synthetic hypothesis about human behavior, not human playtesting, a learned person model, or proof of difficulty.',
        'road_arrows':{'per_site':{'OPEN':3,'MOTEL':6,'GAS':2,'24H':4},'relief_percent_each':8,'spacing_metres':18,'control':'drive over the painted lane; no A input'},
        'duration':'Endless until loss; 4 km is only a comparison benchmark. Simulation capped at 20 km or 2000 seconds.',
        'near_dark':'Seconds with at least 90% tunnel closure.',
        'microsleeps':{'no_sleep_game_over':True,'threshold_sleep_percent':90,'ramp_seconds':30,'interval_seconds':[5,1.2],'closed_seconds':.3,'shutter_seconds':[3/60,4/60],'wake_up':'Every arrow or obstacle contact cancels a blink immediately.','observation':'No road, sign or hazard reads through the closed windshield; dashboard, gravel sound/rumble and remembered trajectories remain available.'},
        'speed_kmh':{'start':62,'minimum':42,'maximum':130,'acceleration':'fatigue-driven, no fixed cruise target','braking':'15 km/h per second while held'},
        'first_neon_metres':650,'neon_gap_metres':[850,1150],
        'impact_damage':{'hit_limit':None,'maximum_added_drift_percent':100,'loss':'Only leaving the road ends a run; hits increase handling damage.'},
    }
    ctx.out.mkdir(exist_ok=True)
    (ctx.out/'human-pilots.json').write_text(json.dumps(report,indent=2)+'\n')
    (ctx.game/'docs/playability.json').write_text(json.dumps(report,indent=2)+'\n')
    rows=['# Synthetic playability study','','128 shared route seeds per profile, 60 Hz game time. These are simulated drivers, not human playtests.','',
          '| Driver | Survives 4 km | Mean distance | Near darkness | Obstacle near misses / drive | Shoulder near misses / drive | Road / sleep / collision losses |',
          '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for p in report['profiles']:
        rows.append(f"| {p['name']} | {p['survives_4km']}/128 | {p['mean_metres']/1000:.2f} km | {p['mean_near_dark_seconds']:.1f} s | {p['mean_obstacle_near_misses']:.2f} | {p['mean_shoulder_near_misses']:.2f} | {p['road_losses']} / {p['sleep_losses']} / {p['collision_losses']} |")
    rows+=['','The full-vision control bypasses the observation mask, including the windshield shutter; it retains fatigue, drifting and the creeping accelerator. Sleep and impact counts never end a run directly, so sleep/collision-count losses are zero; accumulated damage can cause an eventual road loss. Ordinary drivers use remembered trajectories, visible dashboard gauges and gravel cues during blinks, with no visual road/sign/hazard observations. The never-brakes control shares the normal policy and never presses B. The obstacle-only driver brakes for hazards but ignores creeping speed. These are sensitivity comparisons, not real human win rates.','',
           'See `playability.json` for parameters and definitions. A missed light is an unused sign passed; obstacle near misses count distinct resolved encounters, not frames.','']
    (ctx.game/'docs/playability.md').write_text('\n'.join(rows))
    p={r['name']:r for r in report['profiles']}
    good=p['relaxed_human'];cautious=p['overcautious_braking'];control=p['human_full_vision_control']
    print(f"Human model: {good['survives_4km']}/128 survive 4 km; mean {good['mean_metres']/1000:.2f} km, {good['mean_obstacle_near_misses']:.2f} obstacle near misses and {good['mean_near_dark_seconds']:.1f} s near darkness per drive.")
    # Endless runs intentionally end. Compare distance and the consequences
    # of tunnel vision/braking, rather than requiring a removed dawn win.
    # Accidental impacts now wake a no-neon driver, so zero survivors is no
    # longer an invariant. Braking now lets animals clear independently.
    return (good['mean_metres']>=3000 and good['median_near_misses']>=2
        and good['mean_obstacle_near_misses']>=1 and good['mean_near_dark_seconds']>=10
        and cautious['mean_metres']<good['mean_metres']*.8
        and control['road_losses']<good['road_losses'] and control['mean_metres']>good['mean_metres']
        and p['ignores_neon']['mean_metres']<good['mean_metres']*.7
        and p['never_brakes']['mean_metres']<good['mean_metres']*.8)
