#!/usr/bin/env python3
"""Check selected reVC functions remain verbatim, using the read-only reference."""
import hashlib,json,re
from pathlib import Path
here=Path(__file__).resolve().parent
selection=json.loads((here/'source-selection.json').read_text())
reference=Path(selection['reference'])
def function(text,name):
    match=re.search(r'\b'+name+r'\([^;{}]*\)\s*\{',text)
    assert match,name
    pos=match.end();depth=1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
    return text[match.start():pos]
rows=[]
for source,target,key in [('src/fakerw/fake.cpp','fakerw.cpp','fakerw_functions'),('src/rw/RwHelper.cpp','rw_helpers.cpp','rw_helper_functions')]:
    raw=(reference/source).read_bytes();original=raw.decode();local=(here/target).read_text()
    for name in selection[key]:assert function(original,name)==function(local,name),name
    rows.append(dict(source=source,sha256=hashlib.sha256(raw).hexdigest(),functions=len(selection[key])))
print(json.dumps(dict(passed=True,reference_sources=rows),indent=2))
