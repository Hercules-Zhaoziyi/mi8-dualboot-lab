#!/usr/bin/env python3
"""Fetch pinned public porting reference only; never downloads OS inputs or touches a phone."""
from pathlib import Path
import argparse,json,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,default=Path('local/ios-reference'));a=p.parse_args();repo=Path(__file__).resolve().parents[1];out=a.out.resolve()
if not out.is_relative_to((repo/'local').resolve()):raise SystemExit('Output must be under local/')
if out.exists():raise SystemExit('Destination exists; preserve it and review before reuse')
lock=json.loads((repo/'UPSTREAM_SOURCES.json').read_text())['dipper_xnu_reference'];out.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['git','clone','--no-checkout',lock['url'],str(out)],check=True);subprocess.run(['git','-C',str(out),'checkout','--detach',lock['commit']],check=True)
actual=subprocess.check_output(['git','-C',str(out),'rev-parse','HEAD'],text=True).strip();assert actual==lock['commit']
print('Pinned public reference ready. Read docs/build and docs/install. No system payload or phone operation was performed.')
