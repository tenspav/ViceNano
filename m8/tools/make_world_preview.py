#!/usr/bin/env python3
"""Bounded local model sample with UVs and real downsampled RGB565 textures.
All source asset restrictions remain; generated data stays in the build dir.

make_world_preview.py nbdecoshop01.h world_preview.h [preview.bin]
preview.bin ("NVP1", little endian; the same data as the header) rides in
the vcworld.pak runtime pack, where NanoOS's mode 3 reads it (the kernel is
at its 64 KiB cap): char magic[4]; u16 textures, triangles;
u16 texels[textures][64] (8x8 RGB565); triangles x { s16 xyz[9] (1/16),
s16 uv[6] (1/256), u16 texture }.
"""
import re, struct, sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
verts={key:[[float(v.rstrip('f')) for v in row.split(',')]
       for row in re.findall(r'\{([^{}]+)\}',body)]
       for key,body in re.findall(r'NanVertex verts(\d+)\[\]=\{(.*?)\n\};',s,re.S)}
part_tex={key:int(tex) for tex,key in re.findall(r'\{\d+,\d+,(\d+),verts(\d+),ix\d+\}',s)}
groups=[]
for key,body in re.findall(r'uint16_t ix(\d+)\[\]=\{([^}]+)\}',s):
    tri=[];ix=[int(v) for v in body.split(',') if v.strip()]
    for i in range(0,len(ix),3):
        vs=[verts[key][j] for j in ix[i:i+3]]
        if len(vs)!=3 or any(v[8]!=255 for v in vs):continue
        a=[vs[1][j]-vs[0][j] for j in range(3)]
        b=[vs[2][j]-vs[0][j] for j in range(3)]
        c=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
        tri.append((sum(v*v for v in c),vs,part_tex[key]))
    groups.append(sorted(tri,key=lambda t:-t[0]))
tri=[]
while len(tri)<112 and any(groups):
    for g in groups:
        if g and len(tri)<112:tri.append(g.pop(0))
assert len(tri)==112
origin=[(min(v[j] for _,t,_ in tri for v in t)+max(v[j] for _,t,_ in tri for v in t))/2 for j in range(3)]
blob=bytearray()
lines=['// Local user-owned asset sample; do not redistribute.',
       'struct PreviewTriangle { int16_t xyz[9],uv[6]; uint16_t texture; };',
       'static const PreviewTriangle preview[]={']
for _,vs,tex in tri:
    xyz=[round((v[j]-origin[j])*16) for v in vs for j in range(3)]
    uv=[round(v[j]*256) for v in vs for j in (3,4)]
    assert all(-32768<=v<=32767 for v in xyz+uv)
    rgb=[64+round(sum(v[j] for v in vs)/3*191/255) for j in range(5,8)]
    color=(rgb[0]>>3)<<11|(rgb[1]>>2)<<5|(rgb[2]>>3)
    lines.append('{{'+','.join(map(str,xyz))+'},{'+','.join(map(str,uv))+'},'+str(tex)+'},')
    blob+=struct.pack('<9h6hH',*xyz,*uv,tex)
lines.append('};')
packed={key:bytes(int(v) for v in body.split(',') if v.strip()) for key,body in re.findall(r'uint8_t tex(\d+)\[\]=\{([^}]+)\}',s)}
desc=re.findall(r'\{(\d+),(\d+),(\d+),tex(\d+),"[^"]+",(\d+),0x[\da-f]+\}',s)
assert len(desc)<=31 and desc
lines.append('static const uint16_t previewTexels[][64]={')
texels=bytearray()
for ws,hs,fmt,key,size in desc:
    w,h=int(ws),int(hs);data=packed[key]
    assert len(data)==int(size)==512+w*h
    pixels=[]
    for y in range(8):
        for x in range(8):
            index=data[512+min(h-1,(2*y+1)*h//16)*w+min(w-1,(2*x+1)*w//16)]*2
            p=data[index]|data[index+1]<<8
            if fmt=='2': # source RGBA4444 -> RGB565; alpha deferred
                r=(p>>12)&15;g=(p>>8)&15;b=(p>>4)&15
                p=((r*31//15)<<11)|((g*63//15)<<5)|(b*31//15)
            pixels.append(p)
    lines.append('{'+','.join(map(str,pixels))+'},')
    texels+=struct.pack('<64H',*pixels)
lines.append('};')
Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
if len(sys.argv)>3:
    Path(sys.argv[3]).write_bytes(b'NVP1'+struct.pack('<HH',len(desc),len(tri))+texels+blob)
print(f'world model sample: {len(tri)} triangles, {len(desc)} textures at 8x8, UVs preserved')
