#!/usr/bin/env python3
"""Require texture, state badges and native draw progress in captured guest pixels."""
import json
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parent/'run'
rows=[]
for p in sorted(root.glob('cube-*.png')):
 if p.stat().st_mtime < (root/'command.json').stat().st_mtime: continue
 im=Image.open(p).convert('RGB')
 def close(x,y,c,t=12):
  return all(abs(a-b)<=t for a,b in zip(im.getpixel((x,y)),c))
 center=list(im.crop((20,100,220,330)).getdata())
 white=sum(min(c)>230 for c in center)
 red=sum(c[0]>220 and c[1]<30 and c[2]<30 for c in center)
 counter=sum((im.getpixel((96+i*16,406))[0]>128)<<i for i in range(8))
 rows.append(dict(file=p.name,white=white,red=red,counter=counter,base_heap=sum((im.getpixel((10+i*7,422))[0]>128)<<i for i in range(32)),heap=sum((im.getpixel((10+i*7,428))[0]>128)<<i for i in range(32)),
  alpha_reject=close(30,375,(13,20,64)),alpha_accept=close(80,375,(0,255,255)),
  blend=close(130,375,(112,0,112)),no_gl_errors=close(25,406,(0,255,0)),
  heap_within_4096=close(65,406,(0,255,0))))
passed=len(rows)>=3 and all(r['white']>300 and r['red']>300 and all(r[k] for k in ('alpha_reject','alpha_accept','blend','no_gl_errors','heap_within_4096')) for r in rows) and len({r['counter'] for r in rows})>1
result=dict(passed=passed,frames=rows)
(root/'capabilities.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if passed else 1)
