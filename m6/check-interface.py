#!/usr/bin/env python3
"""Check the retained DMAudio signatures and imported enum against read-only reVC."""
import hashlib,json,re
from pathlib import Path
here=Path(__file__).resolve().parent
ref=Path('/home/pavliukov/ipodwork/NanoRework/games/reVC/src/audio')
original=(ref/'DMAudio.h').read_text();reduced=(here/'audio.h').read_text()
def signatures(text):
 result={}
 for ret,name,args in re.findall(r'(void|int32_t|int32|uint8_t|bool8)\s+(\w+)\(([^)]*)\)',text):
  parts=[]
  for arg in args.split(','):
   arg=arg.strip()
   if arg in ('','void'):continue
   arg=re.sub(r'\b\w+$','',arg).strip().replace(' ','')
   for a,b in [('int32_t','int32'),('uint32_t','uint32'),('uint16_t','uint16'),('uint8_t','uint8'),('bool8','uint8')]:arg=arg.replace(a,b)
   parts.append(arg)
  result[name]=(ret.replace('int32_t','int32').replace('uint8_t','uint8').replace('bool8','uint8'),parts)
 return result
want=signatures(original);got=signatures(reduced[reduced.index("class cDMAudio"):reduced.index("extern cDMAudio")])
assert got and all(want.get(name)==signature for name,signature in got.items()),[(n,s,want.get(n)) for n,s in got.items() if want.get(n)!=s]
enums=(ref/'audio_enums.h').read_text();a=enums.index('enum eAudioType');enum=enums[a:enums.index('};',a)+2];assert enum in reduced
result={'pass':True,'reference_header_sha256':hashlib.sha256(original.encode()).hexdigest(),'eAudioType_verbatim':True,'retained_signatures':got,'scope':'Reduced contract only; omitted full AudioManager methods are not linked.'}
(here/'source-check.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS:',len(got),'DMAudio signatures and verbatim eAudioType')
