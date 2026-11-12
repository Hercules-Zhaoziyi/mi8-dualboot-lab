#!/usr/bin/env python3
"""Create PRIVATE local font resources. Outputs must never be uploaded."""
from pathlib import Path
import argparse,hashlib,json,subprocess
from fontTools.ttLib import TTFont
from fontTools import subset

GUIDS={'regular':'e71a7a5c-8196-4d48-b641-09d21a13bdf4','bold':'30c1b010-8e7b-48a7-bd44-6978cf57d210'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--regular',type=Path,required=True);p.add_argument('--bold',type=Path,required=True);p.add_argument('--out',type=Path,default=Path('local/fonts'));p.add_argument('--base-tools',type=Path);a=p.parse_args()
 repo=Path(__file__).resolve().parents[1];out=a.out.resolve();assert out.is_relative_to((repo/'local').resolve()),'Private outputs must be under this repository local/'
 out.mkdir(parents=True,exist_ok=True);source=repo/'source/simple-init/src/gui/interface/core/bootmenu_hercules.inc';points=set(range(32,127))|{ord(c) for c in source.read_text() if ord(c)>127};report={}
 for name,src in [('regular',a.regular),('bold',a.bold)]:
  font=TTFont(src,fontNumber=0,recalcTimestamp=False);flags=font['OS/2'].fsType
  if flags&0x102:raise SystemExit('Input embedding/subsetting flags require a different authorized font')
  opts=subset.Options();opts.name_IDs=['*'];opts.name_languages=['*'];opts.name_legacy=True;opts.layout_features=[];worker=subset.Subsetter(options=opts);worker.populate(unicodes=points);worker.subset(font);font.recalcTimestamp=False
  target=out/f'hercules-ui-{name}.ttf';font.save(target);check=TTFont(target);missing=points-set(check.getBestCmap());assert not missing,f'Missing characters: {missing}'
  report[name]={'guid':GUIDS[name],'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'bytes':target.stat().st_size,'fsType':flags}
  if a.base_tools:
   sec=out/f'{name}.raw';subprocess.run([str(a.base_tools/'GenSec'),'-s','EFI_SECTION_RAW','-o',str(sec),str(target)],check=True)
   subprocess.run([str(a.base_tools/'GenFfs'),'-t','EFI_FV_FILETYPE_FREEFORM','-g',GUIDS[name],'-s','-i',str(sec),'-o',str(out/f'HerculesFont-{name}.ffs')],check=True)
 (out/'PRIVATE-FONT-MANIFEST.json').write_text(json.dumps(report,indent=2));(out/'DO-NOT-UPLOAD.txt').write_text('Private licensed font resources. Do not commit or upload this directory.\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
