#!/usr/bin/env python3
from pathlib import Path
import argparse,os,subprocess,json
def main():
 p=argparse.ArgumentParser();p.add_argument('--simple-init',type=Path,default=Path('source/simple-init'));p.add_argument('--local-fonts',type=Path);p.add_argument('--out',type=Path,default=Path('local/tests'));a=p.parse_args();repo=Path(__file__).resolve().parents[1]
 if not a.local_fonts or not all((a.local_fonts/f'hercules-ui-{v}.ttf').is_file() for v in ['regular','bold']):
  print(json.dumps({'render_tests':'SKIPPED','reason':'Provide your authorized local font pack with --local-fonts; no substitute UI typography is used.'}));raise SystemExit(77)
 si=a.simple_init.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);inc=out/'include';inc.mkdir(exist_ok=True)
 (inc/'lv_conf.h').write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#include <stdlib.h>\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_CUSTOM 1\n#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>\n#define LV_MEM_CUSTOM_ALLOC malloc\n#define LV_MEM_CUSTOM_FREE free\n#define LV_MEM_CUSTOM_REALLOC realloc\n#define LV_TICK_CUSTOM 0\n#define LV_USE_LOG 1\n#define LV_LOG_PRINTF 1\n#endif\n')
 (out/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.16)
project(HerculesUiTests C)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)
include_directories("{inc}" "{si}/libs/lvgl" "{si}/libs/stb" "{si}/libs/nanosvg/src" "{si}/libs/freetype/include" "{si}/src/gui/interface/core")
set(CMAKE_C_FLAGS "-O1 -g -fsanitize=address -fno-omit-frame-pointer")
file(GLOB_RECURSE LV_SOURCES "{si}/libs/lvgl/src/*.c")
add_library(lvgl STATIC ${{LV_SOURCES}})
add_executable(test_menu "{repo}/tests/test_menu.c")
target_link_libraries(test_menu lvgl m -l:libfreetype.so.6)
''')
 subprocess.run(['cmake','-S',str(out),'-B',str(out/'build')],check=True);subprocess.run(['cmake','--build',str(out/'build'),'-j6'],check=True)
 env=os.environ.copy();env['HERCULES_TEST_FONTS']=str(a.local_fonts.resolve());env['HERCULES_SIMPLEINIT_SOURCE']=str(si)
 subprocess.run([str(out/'build/test_menu'),str(si/'root/usr/share/pixmaps/simple-init'),str(out)],env=env,check=True)
 print(json.dumps({'render_tests':'PASS','asan':'PASS','font_data':'local-only, not uploaded'}))
if __name__=='__main__':main()
