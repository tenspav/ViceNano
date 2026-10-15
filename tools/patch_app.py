#!/usr/bin/env python3
"""Generate a read-only NAND overlay replacing the existing Mario Kart app.
The original FAT directory and cluster chain remain unchanged.
"""
import importlib.util
from pathlib import Path
import struct,subprocess,sys
source,app,output=map(Path,sys.argv[1:])
tool=Path('/home/pavliukov/ipodwork/NanoRework/emulator/tools/disk_patch.py')
spec=importlib.util.spec_from_file_location('disk_patch',tool);dp=importlib.util.module_from_spec(spec);spec.loader.exec_module(dp)
subprocess.run(['python3',str(tool),str(source),'/Apps/Executables/Mario Kart.hbapp='+str(app),'-o',str(output)],check=True)
print('Renderer replaces Mario Kart only inside the NAND overlay; source opened read-only')
