#!/usr/bin/env python3
import argparse,hashlib,json,struct,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--assets',type=Path,default=Path('/home/pavliukov/ipodwork/NanoRework/games/vc-files'));p.add_argument('--model',default='trafficcone');p.add_argument('--txd',default='dyncones');a=p.parse_args()
assert all(re.fullmatch(r'[A-Za-z0-9_-]+',x) for x in (a.model,a.txd)), 'Use plain IMG entry basenames'
out=Path(__file__).resolve().parent/'generated';out.mkdir(exist_ok=True)
entries={}
raw=(a.assets/'models/gta3.dir').read_bytes();assert len(raw)%32==0
for off in range(0,len(raw),32):
 sector,count,name=struct.unpack_from('<II24s',raw,off);entries[name.split(b'\0')[0].decode().lower()]=(sector,count)
report={}
with (a.assets/'models/gta3.img').open('rb') as f:
 for name in (a.model+'.dff',a.txd+'.txd'):
  sector,count=entries[name.lower()];f.seek(sector*2048);data=f.read(count*2048);assert len(data)==count*2048
  chunk,size,version=struct.unpack_from('<III',data);data=data[:12+size];(out/name).write_bytes(data)
  report[name]=dict(sector=sector,sectors=count,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),chunk=chunk,version=version)
(out/'selection.json').write_text(json.dumps([a.model+'.dff',a.txd+'.txd'])+'\n')
(out/'extraction.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
