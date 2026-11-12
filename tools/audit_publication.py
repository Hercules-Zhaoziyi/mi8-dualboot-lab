#!/usr/bin/env python3
"""Fail closed on private payloads and credentials in the public source tree."""
from pathlib import Path
import argparse,re,json,hashlib
DENIED={'.img','.ipsw','.apfs','.dmg','.ipa','.ttc','.p12','.pfx','.key','.pem','.o','.a','.dll','.exe','.dylib','.efi','.ffs','.bin'}
TOKEN=re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path,nargs='?',default=Path('.'));p.add_argument('--report',type=Path);a=p.parse_args();root=a.root.resolve();errors=[];count=0
 for f in root.rglob('*'):
  rel=f.relative_to(root)
  if any(v in rel.parts for v in ['.git','local','dist','__pycache__','build']):continue
  if f.is_symlink():
   if not f.resolve().is_relative_to(root):errors.append({'path':str(rel),'reason':'symlink leaves source tree'})
   continue
  if not f.is_file():continue
  count+=1
  if f.suffix.lower() in DENIED:errors.append({'path':str(rel),'reason':'binary/private payload type excluded from Git'})
  if f.suffix.lower()=='.ttf' and f.name not in ['wqy-mono-microhei.ttf','fontawesome5.ttf']:errors.append({'path':str(rel),'reason':'UI fonts must be provided locally'})
  data=f.read_bytes()
  if TOKEN.search(data):errors.append({'path':str(rel),'reason':'credential/private-key marker'})
  if f.suffix.lower() in ['.md','.py','.sh','.ps1','.json','.cfg','.inc'] and (b'wx'+b'id_') in data:errors.append({'path':str(rel),'reason':'private messaging identifier'})
 result={'files_reviewed':count,'status':'FAIL' if errors else 'PASS','errors':errors}
 print(json.dumps(result,ensure_ascii=False,indent=2))
 if a.report:a.report.write_text(json.dumps(result,indent=2))
 if errors:raise SystemExit(1)
if __name__=='__main__':main()
