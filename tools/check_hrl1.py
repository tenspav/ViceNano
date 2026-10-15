#!/usr/bin/env python3
"""Validate the freestanding C++ constructor representation in a packed HRL1 app."""
from pathlib import Path
import struct,sys
from elftools.elf.elffile import ELFFile
app=Path(sys.argv[1]).read_bytes();magic,entry,image,span,count,align=struct.unpack_from('<6I',app)
assert magic==0x314c5248 and len(app)==24+image+count*4 and entry<image<=span
with open(sys.argv[2],'rb') as f:
 elf=ELFFile(f);symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
 first,last=symbols['__init_array_start'],symbols['__init_array_end'];assert first<=last<=image
 for off in range(first,last,4):
  rel,=struct.unpack_from('<i',app,24+off);target=(off+rel)&0xffffffff
  assert target&1 and target<image,(hex(off),hex(target))
 print(f'PASS: HRL1 bounds, {count} relocations, {(last-first)//4} packed relative constructors')
