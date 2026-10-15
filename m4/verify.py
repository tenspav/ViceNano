#!/usr/bin/env python3
import json
from pathlib import Path
from PIL import Image,ImageChops
root=Path(__file__).resolve().parent/'run';rows=[];frames=[]
for p in sorted(root.glob('vcmodel-*.png')):
 if p.stat().st_mtime < (root/'command.json').stat().st_mtime: continue
 im=Image.open(p).convert('RGB');frames.append(im)
 pixels=list(im.crop((10,50,230,360)).getdata())
 red=sum(c[0]>180 and c[1]<80 and c[2]<80 for c in pixels);white=sum(min(c)>180 for c in pixels)
 rows.append(dict(file=p.name,red_pixels=red,white_pixels=white,
 no_errors=im.getpixel((20,406))==(0,255,0),balanced_unloads=im.getpixel((55,406))==(0,255,0),supported=im.getpixel((90,406))==(0,255,0),
 cycles=sum((im.getpixel((117+i*15,406))[0]>128)<<i for i in range(8)),
 rw_baseline=sum((im.getpixel((10+i*7,414))[0]>128)<<i for i in range(32)),
 unloaded_heap=sum((im.getpixel((10+i*7,418))[0]>128)<<i for i in range(32)),
 rw_live=sum((im.getpixel((10+i*7,422))[0]>128)<<i for i in range(32)),
 heap=sum((im.getpixel((10+i*7,428))[0]>128)<<i for i in range(32))))
changes=[sum(c!=(0,0,0) for c in ImageChops.difference(frames[0],im).getdata()) for im in frames[1:]]
passed=len(rows)==8 and all(r['red_pixels']>100 and r['white_pixels']>100 and r['no_errors'] and r['balanced_unloads'] and r['supported'] and r['rw_live']>0 and r['heap']>0 for r in rows) and len({r['rw_live'] for r in rows})==1 and max(r['heap'] for r in rows)-min(r['heap'] for r in rows)<=256 and max(r['unloaded_heap'] for r in rows)-min(r['unloaded_heap'] for r in rows)<=256 and max(r['cycles'] for r in rows)>min(r['cycles'] for r in rows) and max(changes)>1000
result=dict(passed=passed,frames=rows,changed_pixels=changes,heap_range=max((r['heap'] for r in rows),default=0)-min((r['heap'] for r in rows),default=0));(root/'capabilities.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if passed else 1)
