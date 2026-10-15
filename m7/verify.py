#!/usr/bin/env python3
"""Decode screenshot telemetry and assert the M7 reduced-viewer input gate."""
from pathlib import Path
import json
from PIL import Image,ImageChops
root=Path(__file__).resolve().parent/'pass'
fields=['touch_events','hid_reports','player_x16','player_z16','yaw100','pitch100','rw_live','os_free','rw_peak','errors','allocation_failures','bt_state','cross','left_x','left_y','frame']
def read(name):
 im=Image.open(root/(name+'.png')).convert('RGB')
 d={'file':name+'.png'}
 for r,k in enumerate(fields):
  v=sum((im.getpixel((10+i*7,149+r*4))[0]>128)<<i for i in range(32))
  if k in ['player_x16','player_z16','yaw100','pitch100'] and v>=2**31:v-=2**32
  d[k]=v
 return d
names=['initial','before','touch-moved','touch-camera']+[p.stem for p in sorted(root.glob('vcinput-*.png'),key=lambda p:int(p.stem.split('-')[-1]))]
rows=[read(n) for n in names]
initial,before,moved,camera=rows[:4]
checks={
 '500_observed_touch_events':moved['touch_events']-before['touch_events']>=500,
 'touch_moves_world_marker':abs(moved['player_x16']-before['player_x16'])+abs(moved['player_z16']-before['player_z16'])>16,
 'touch_rotates_camera':abs(camera['yaw100']-moved['yaw100'])>100,
 'no_touch_heap_growth':before['os_free']-moved['os_free']<=512 and moved['rw_live']==before['rw_live'],
 'secure_hid_reports':max(r['hid_reports'] for r in rows)>=7 and any(r['bt_state']==4 for r in rows),
 'hid_moves_world_marker':any(r['hid_reports']>=2 and abs(r['player_x16']-camera['player_x16'])+abs(r['player_z16']-camera['player_z16'])>16 for r in rows[4:]),
 'hid_rotates_camera':any(r['hid_reports']>=4 and abs(r['yaw100']-camera['yaw100'])>100 for r in rows[4:]),
 'no_guest_errors':all(r['errors']==0 and r['allocation_failures']==0 for r in rows),
 'within_budget':all(r['rw_peak']<18*1024*1024 for r in rows),
}
log=(root/'emulator.log').read_text()
checks['pairing_encryption_sdp']=all(s in log for s in ['paired with "NanoPad"','link encryption on','L2CAP PSM 0x13 open'])
source=json.loads((root/'check.json').read_text())
checks['source_disk_unchanged']=source['source_size_mtime_and_replaced_block_hashes_unchanged']
a=Image.open(root/'before.png').convert('RGB').crop((0,40,240,300))
b=Image.open(root/'touch-moved.png').convert('RGB').crop((0,40,240,300))
changes=sum(c!=(0,0,0) for c in ImageChops.difference(a,b).getdata())
checks['scene_changes']=changes>1000
checks['render_states_supported']=all(Image.open(root/(n+'.png')).convert('RGB').getpixel((90,136))==(0,255,0) for n in names)
checks['balanced_budget']=all(Image.open(root/(n+'.png')).convert('RGB').getpixel((55,136))==(0,255,0) for n in names)
result={'pass':all(checks.values()),'checks':checks,'observed_touch_events':moved['touch_events']-before['touch_events'],'touch_heap_difference':moved['os_free']-before['os_free'],'scene_changed_pixels':changes,'frames':rows}
(root/'capabilities.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2));raise SystemExit(0 if result['pass'] else 1)
