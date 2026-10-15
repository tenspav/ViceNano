#!/usr/bin/env python3
"""Decode on-screen guest counters; reject drift, errors, missing views or cycles."""
import json
from pathlib import Path
from PIL import Image,ImageChops
root=Path(__file__).resolve().parent/'run'
fields=['rw_baseline','unloaded_heap','rw_live','heap','loads','evictions','active','rw_peak','baseline_low','baseline_high','audits','max_state_heap_range','heap_low','resident_mask','allocation_failures','frame']
rows=[];frames=[]
for p in sorted(root.glob('vcstream-*.png'),key=lambda p:int(p.stem.split('-')[-1])):
 if p.stat().st_mtime<(root/'command.json').stat().st_mtime:continue
 im=Image.open(p).convert('RGB');frames.append(im)
 pixels=list(im.crop((10,30,230,340)).getdata());colored=[c for c in pixels if sum(abs(x-y) for x,y in zip(c,(13,20,64)))>30]
 r={'file':p.name,'model_pixels':len(colored),'model_colors':len(set(colored)),
 'no_errors':im.getpixel((20,356))==(0,255,0),'balanced':im.getpixel((55,356))==(0,255,0),'supported':im.getpixel((90,356))==(0,255,0)}
 for row,key in enumerate(fields):r[key]=sum((im.getpixel((10+i*7,369+row*4))[0]>128)<<i for i in range(32))
 rows.append(r)
changes=[sum(c!=(0,0,0) for c in ImageChops.difference(frames[0].crop((0,0,240,340)),im.crop((0,0,240,340))).getdata()) for im in frames[1:]]
check=json.loads((root/'check.json').read_text())
passed=len(rows)>=8 and check['source_size_mtime_and_replaced_block_hashes_unchanged'] and all(
 r['model_pixels']>1000 and r['model_colors']>32 and r['no_errors'] and r['balanced'] and r['supported'] and
 r['rw_baseline']>0 and r['rw_peak']<18*1024*1024 and r['allocation_failures']==0 and r['heap']>0 and r['baseline_low']!=0xffffffff and
 0<=r['baseline_high']-r['baseline_low']<=512 and r['max_state_heap_range']<=8192 for r in rows) and max(r['loads'] for r in rows)>=25 and max(r['evictions'] for r in rows)>=20 and max(r['audits'] for r in rows)>=3 and len({r['active'] for r in rows})>=2 and all(b['frame']>a['frame'] for a,b in zip(rows,rows[1:])) and max(changes)>1000
summary={}
if rows:
 summary={'captured_frames':[rows[0]['frame'],rows[-1]['frame']],
 'max_loads':max(r['loads'] for r in rows),'max_evictions':max(r['evictions'] for r in rows),
 'full_unload_audits':max(r['audits'] for r in rows),'rw_peak':max(r['rw_peak'] for r in rows),
 'unloaded_heap_min':min(r['baseline_low'] for r in rows),'unloaded_heap_max':max(r['baseline_high'] for r in rows),
 'max_resident_set_heap_range':max(r['max_state_heap_range'] for r in rows),
 'minimum_guest_free_heap':min(r['heap_low'] for r in rows),
 'resident_sets':sorted({r['resident_mask'] for r in rows})}
result={'summary':summary,'passed':passed,'frames':rows,'changed_scene_pixels':changes,'source_unchanged':check['source_size_mtime_and_replaced_block_hashes_unchanged']}
(root/'capabilities.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if passed else 1)
