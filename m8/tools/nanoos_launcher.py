#!/usr/bin/env python3
"""Generate M8 integration; sibling NanoOS sources remain driver-owned.
The M8 kernel keeps DevMode stats but omits its unrelated driver-probe page.
"""
import pathlib, re, sys
s = pathlib.Path(sys.argv[1]).read_text()
s=s.replace('#include "drivers.h"\n','')
s=re.sub(r'^.*unsigned drivers_page = 0;[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^.*mbx_test_leave\(\);\n','',s,flags=re.M)  # GPU test page only
s=s.replace('scene = 0; drivers_page = 0;', 'scene = 0;')
start=s.find('else if (scene == DEVMODE) {')
if start>=0:
    brace=s.index('{',start); depth=1; end=brace+1
    while depth:
        if s[end]=='{': depth+=1
        if s[end]=='}': depth-=1
        end+=1
    s=s[:start]+'else if (scene == DEVMODE && lcd_ready()) devmode_draw();'+s[end:]
replacements = {
 '#include "lcd.h"': '#include "lcd.h"\n#include "nanoos_shim.h"',
 'if (keys & BUTTON_HOME) { scene = 0; launcher(selected); }':
 'if ((keys & BUTTON_HOME) && !(scene == 1 && m8_app_home(keys))) { unsigned m8_saved = scene == 1 && m8_app_exit(); if (m8_saved) selected = 0; scene = 0; launcher(selected); if (m8_saved) m8_launcher_hint(); }',
 'else if (scene == DEVMODE && lcd_ready()) devmode_draw();':
 'else if (scene == 1) { if (m8_app_tick(keys)) { m8_app_exit(); selected = 0; scene = 0; launcher(selected); m8_launcher_hint(); } }\n            else if (scene == DEVMODE && lcd_ready()) devmode_draw();',
 'else placeholder(scene);': 'else if(scene == 1) m8_app_enter();\n                else placeholder(scene);',
}
for old, new in replacements.items():
    if s.count(old) != 1:
        raise SystemExit(f'NanoOS launcher changed; expected one occurrence of {old!r}')
    s = s.replace(old, new)
pathlib.Path(sys.argv[2]).write_text(s)
