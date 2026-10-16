#!/usr/bin/env python3
"""LZSS: u32 decoded size, groups of eight tokens (1 literal, 0 match).
Match u16: distance-1 in high 12 bits, length-3 in low 4; length 18 adds an extension byte. Round-trip checked.
"""
import struct, sys
from collections import defaultdict, deque
from pathlib import Path
src=Path(sys.argv[1]).read_bytes()
# Find maximum matches at every byte, then minimize encoded token cost.
# Costs are eighths of bytes (each token also consumes one flag bit).
# This avoids greedy matches hiding better matches one byte later.
positions=defaultdict(deque)
matches=[]
for i in range(len(src)):
    q=positions[src[i:i+3]]
    while q and i-q[0]>4096:q.popleft()
    best=0;distance=0
    for start in list(q)[-512:]:
        n=0
        while n<273 and i+n<len(src) and src[start+n]==src[i+n]:n+=1
        if n>best:best=n;distance=i-start
    matches.append((best,distance))
    q.append(i)
cost=[0]*(len(src)+1);choice=[1]*len(src)
for i in range(len(src)-1,-1,-1):
    cost[i]=9+cost[i+1]
    length,_=matches[i]
    for n in range(3,length+1):
        c=(17 if n<18 else 25)+cost[i+n]
        if c<cost[i]:cost[i]=c;choice[i]=n
out=bytearray(struct.pack('<I',len(src)))
i=0
while i<len(src):
    flagpos=len(out);out.append(0);flags=0
    for bit in range(8):
        if i==len(src):break
        n=choice[i]
        if n>=3:
            distance=matches[i][1]
            out.extend(struct.pack('<H',((distance-1)<<4)|(min(n,18)-3)))
            if n>=18:out.append(n-18)
        else:flags|=1<<bit;out.append(src[i])
        i+=n
    out[flagpos]=flags
# Independent decoder verifies every token and all output bytes before writing.
p=4;decoded=bytearray()
while len(decoded)<len(src):
    flags=out[p];p+=1
    for bit in range(8):
        if len(decoded)==len(src):break
        if flags&(1<<bit):decoded.append(out[p]);p+=1
        else:
            token=out[p]|out[p+1]<<8;p+=2
            distance=(token>>4)+1;n=(token&15)+3
            if n==18:n+=out[p];p+=1
            assert distance<=len(decoded) and len(decoded)+n<=len(src)
            for _ in range(n):decoded.append(decoded[-distance])
assert decoded==src and p==len(out)
out.extend(bytes((-len(out))%4))
Path(sys.argv[2]).write_bytes(out)
print(f'LZSS kernel: {len(src)} -> {len(out)} bytes; round trip passed')
