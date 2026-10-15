#!/usr/bin/env python3
"""Check retained controller field names/types against the read-only reference."""
from pathlib import Path
import re,json,hashlib
here=Path(__file__).resolve().parent
ref=Path('/home/pavliukov/ipodwork/NanoRework/games/reVC/src/core/Pad.h')
s=ref.read_text().split('class CMouseControllerState')[0]
r=(here/'input.h').read_text().split('struct CPad')[0]
def fields(t,typ):
 return [n.strip() for decl in re.findall(r'\b'+typ+r'\s+([^;()]+);',t) for n in decl.split(',')]
a=fields(s,'int16');b=fields(r,'int16_t')
assert b and set(b)<=set(a)
result={'reference':str(ref),'reference_sha256':hashlib.sha256(ref.read_bytes()).hexdigest(),'retained_int16_fields':b,'all_match':True,'abi_compatible_layout':False,'omitted':['NetworkTalk','full gameplay CPad methods']}
(here/'source-check.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS:',len(b),'retained int16 controller fields')
