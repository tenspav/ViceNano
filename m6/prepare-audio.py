#!/usr/bin/env python3
"""Read-only local VC audio -> three PCM SFX and independent 256-sample IMA blocks."""
import hashlib,json,struct,subprocess
from pathlib import Path
here=Path(__file__).resolve().parent; out=here/'generated';out.mkdir(exist_ok=True)
source=Path('/home/pavliukov/ipodwork/NanoRework/games/vc-files/Audio')
steps=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
index=[-1,-1,-1,-1,2,4,6,8]; headers=['#pragma once\n#include <stdint.h>']; report=[]
def decode(name,duration):
 raw=(source/name).read_bytes();data=bytes(v^0x22 for v in raw) if name.lower().endswith('.adf') else raw
 p=subprocess.run(['ffmpeg','-v','error','-i','pipe:0','-t',str(duration),'-f','s16le','-ac','1','-ar','8000','pipe:1'],input=data,stdout=subprocess.PIPE,check=True)
 report.append({'source':name,'source_sha256':hashlib.sha256(raw).hexdigest(),'samples':len(p.stdout)//2})
 return list(struct.unpack('<%dh'%(len(p.stdout)//2),p.stdout))
def array(name,typ,values):headers.append('static const '+typ+' '+name+'[]={'+','.join(map(str,values))+'};')
for i,name in enumerate(['AirHornL.wav','BikeRev.wav','CameraL.wav']):array('sfx%d'%i,'int16_t',decode(name,.4))
samples=decode('FEVER.adf',60); assert len(samples)==480000
packed=bytearray()
for start in range(0,len(samples),256):
 chunk=samples[start:start+256];pred=chunk[0];ix=0;packed+=struct.pack('<hBB',pred,ix,0);nibbles=[]
 for target in chunk: # includes predictor: exactly 256 decoded nibbles per block
  step=steps[ix];d=target-pred;code=8 if d<0 else 0;d=abs(d);delta=step>>3
  for bit,part in [(4,step),(2,step>>1),(1,step>>2)]:
   if d>=part:code|=bit;d-=part;delta+=part
  pred=max(-32768,min(32767,pred+(-delta if code&8 else delta)));ix=max(0,min(88,ix+index[code&7]));nibbles.append(code)
 for j in range(0,256,2):packed.append(nibbles[j]|nibbles[j+1]<<4)
array('radioData','uint8_t',packed)
headers.append('static const unsigned radioSamples=480000;')
(out/'audio.h').write_text('\n'.join(headers)+'\n');(here/'asset-provenance.json').write_text(json.dumps({'assets':report,'radio_bytes':len(packed),'rate':8000,'format':'independent 256-sample IMA blocks (4-byte header + 128 bytes)'},indent=2)+'\n')
