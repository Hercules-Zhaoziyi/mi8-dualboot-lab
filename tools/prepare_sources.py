#!/usr/bin/env python3
"""Fetch pinned OPEN platform build dependencies; never downloads Apple OS data."""
from pathlib import Path
import argparse,json,subprocess
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--dest',type=Path,default=Path('local/edk2-sdm845'));a=p.parse_args();repo=Path(__file__).resolve().parents[1];lock=json.loads((repo/'UPSTREAM_SOURCES.json').read_text());item=lock['edk2_port'];dest=a.dest.resolve()
 assert dest.is_relative_to((repo/'local').resolve()),'Dependencies must be under local/'
 if not dest.exists():subprocess.run(['git','clone','--no-checkout',item['url'],str(dest)],check=True)
 if subprocess.run(['git','-C',str(dest),'status','--porcelain'],capture_output=True,text=True,check=True).stdout.strip():raise SystemExit('Existing dependency has changes; preserve and review them before switching')
 subprocess.run(['git','-C',str(dest),'checkout','--detach',item['commit']],check=True);subprocess.run(['git','-C',str(dest),'submodule','update','--init','--recursive'],check=True)
 for key,path in [('edk2','Common/edk2'),('edk2_platforms','Common/edk2-platforms')]:
  actual=subprocess.check_output(['git','-C',str(dest/path),'rev-parse','HEAD'],text=True).strip();assert actual==lock[key]['commit'],f'{key} commit mismatch'
 print('Pinned dependencies ready. No phone command was executed.')
if __name__=='__main__':main()
