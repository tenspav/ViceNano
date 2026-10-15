#!/usr/bin/env python3
"""Verify guest telemetry, exact mixed PCM and chronological minute-long radio output."""
import hashlib,json,subprocess,wave
from array import array
from pathlib import Path
from PIL import Image
here=Path(__file__).resolve().parent;root=here/'run'
fields='blocks frames mix_us max_mix_us voices_peak starts stops radio_reads radio_ends failures live peak cycles heap_low heap_high frame'.split();rows=[]
for p in sorted(root.glob('vcaudio-*.png'),key=lambda p:int(p.stem.split('-')[-1])):
 assert p.stat().st_mtime>=(root/'command.json').stat().st_mtime
 im=Image.open(p).convert('RGB');r={'file':p.name}
 for row,key in enumerate(fields):r[key]=sum((im.getpixel((10+i*7,369+row*4))[0]>128)<<i for i in range(32))
 r['render_ok']=all(im.getpixel((x,356))==(0,255,0) for x in [20,55,90]);rows.append(r)
subprocess.run(['c++','-std=c++11','-O2','-DAUDIO_HOST',str(here/'audio.cpp'),str(here/'capture-reference.cpp'),'-o',str(here/'capture-reference')],check=True)
mixed=subprocess.check_output([str(here/'capture-reference')]);music=subprocess.check_output([str(here/'capture-reference'),'radio'])
with wave.open(str(root/'audio.wav'),'rb') as w:
 assert (w.getnchannels(),w.getsampwidth(),w.getframerate())==(2,2,44100)
 pcm=w.readframes(w.getnframes())
match=pcm.find(mixed);positions=[];after=0
# A short exact fingerprint in every source second, searched in order. Multiple
# offsets allow the scheduled SFX to cover part of a second without hiding radio.
for second in range(60):
 candidates=[]
 for tenth in range(10):
  offset=second*44100+tenth*4410
  fingerprint=music[offset*4:(offset+256)*4]
  if not any(fingerprint):continue
  pos=pcm.find(fingerprint,after)
  if pos>=0:candidates.append((pos,offset))
 if not candidates:positions.append({'second':second,'capture_frame':-1});continue
 pos,offset=min(candidates);positions.append({'second':second,'source_frame':offset,'capture_frame':pos//4});after=pos+1024
values=array('h');values.frombytes(pcm);peak=max(map(abs,values));check=json.loads((root/'check.json').read_text())
last=rows[-1];mean=last['mix_us']/last['blocks'];period=8192/44100*1e6
summary={'audio_bytes':len(pcm),'audio_seconds':len(pcm)/176400,'peak':peak,'exact_mixed_block_offset':match,
 'radio_seconds_matched_in_order':sum(p['capture_frame']>=0 for p in positions),'radio_alignment_offsets':sorted({p['capture_frame']-p['source_frame'] for p in positions if p['capture_frame']>=0}),'simultaneous_voices':last['voices_peak'],
 'radio_eof_count':last['radio_ends'],'cycles':last['cycles'],'audio_live_bytes':last['live'],'audio_peak_bytes':last['peak'],
 'world_unloaded_heap_low':last['heap_low'],'world_unloaded_heap_high':last['heap_high'],
 'world_unloaded_heap_range':last['heap_high']-last['heap_low'],'mean_mix_us_per_8192_frames':mean,
 'max_mix_us_per_8192_frames':last['max_mix_us'],'emulated_mean_mix_percent':100*mean/period,'emulated_max_mix_percent':100*last['max_mix_us']/period,
 'app_sha256':hashlib.sha256((here/'vcaudio.hbapp').read_bytes()).hexdigest(),'capture_sha256':hashlib.sha256((root/'audio.wav').read_bytes()).hexdigest()}
passed=len(rows)>=8 and check['pass'] and match>=0 and peak>1000 and summary['radio_seconds_matched_in_order']==60 and len(summary['radio_alignment_offsets'])==1 and all(
 r['render_ok'] and r['failures']==0 and r['voices_peak']>=3 and r['radio_ends']>=1 and r['peak']==131184 and r['live']==131184 and r['starts']==r['stops']+1 and r['stops']==r['cycles'] and r['heap_low']>0 and 0<=r['heap_high']-r['heap_low']<=1024 for r in rows) and last['cycles']>=20 and all(b['frame']>a['frame'] and b['frames']>a['frames'] for a,b in zip(rows,rows[1:])) and last['max_mix_us']<period
result={'pass':passed,'summary':summary,'frames':rows,'radio_fingerprints':positions,'disk_unchanged':check['source_size_mtime_and_replaced_block_hashes_unchanged']}
(root/'capabilities.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'pass':passed,'summary':summary},indent=2));raise SystemExit(0 if passed else 1)
