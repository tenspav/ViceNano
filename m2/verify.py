#!/usr/bin/env python3
import json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parent/'run'
rows=[]
for p in sorted(root.glob('rwcube-*.png')):
 if p.stat().st_mtime < (root/'command.json').stat().st_mtime: continue
 im=Image.open(p).convert('RGB');px=list(im.crop((10,80,230,335)).getdata())
 white=sum(min(c)>225 for c in px);red=sum(c[0]>220 and c[1]<30 and c[2]<30 for c in px)
 heap=sum((im.getpixel((10+i*7,422))[0]>128)<<i for i in range(32))
 baseline=sum((im.getpixel((10+i*7,428))[0]>128)<<i for i in range(32))
 overlay=im.getpixel((40,375))
 rows.append(dict(file=p.name,white=white,red=red,heap=heap,baseline=baseline,
 no_errors=im.getpixel((25,406))==(0,255,0),supported=im.getpixel((65,406))==(0,255,0),
 overlay=overlay,overlay_blend=overlay[0]<30 and 100<overlay[1]<160 and 120<overlay[2]<180))
passed=len(rows)==8 and all(r['white']>300 and r['red']>300 and r['no_errors'] and r['supported'] and r['overlay_blend'] and r['heap']>0 and abs(r['heap']-r['baseline'])<8192 for r in rows)
result=dict(passed=passed,frames=rows);(root/'capabilities.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit(0 if passed else 1)
