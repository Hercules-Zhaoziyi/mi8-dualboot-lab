#!/usr/bin/env python3
"""Inspect a locally authorized IPA without extracting, signing or uploading it."""
from pathlib import Path
import argparse,zipfile,plistlib,struct,json
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('ipa',type=Path);a=p.parse_args()
 with zipfile.ZipFile(a.ipa) as z:
  apps=[n for n in z.namelist() if n.startswith('Payload/') and n.count('/')==2 and n.endswith('.app/Info.plist')];assert len(apps)==1
  info=plistlib.loads(z.read(apps[0]));base=apps[0].rsplit('/',1)[0];data=z.read(base+'/'+info['CFBundleExecutable']);encrypted=[]
  if data[:4]==bytes.fromhex('cffaedfe'):
   ncmds=struct.unpack_from('<I',data,16)[0];pos=32
   for i in range(ncmds):
    cmd,size=struct.unpack_from('<II',data,pos);assert size>=8 and pos+size<=len(data)
    if cmd in (0x21,0x2c):encrypted.append(struct.unpack_from('<I',data,pos+16)[0])
    pos+=size
  print(json.dumps({'bundle_id':info.get('CFBundleIdentifier'),'version':info.get('CFBundleShortVersionString'),'minimum_os':info.get('MinimumOSVersion'),'unpacked_bytes':sum(v.file_size for v in z.infolist()),'encryption_cryptid':encrypted,'note':'Signing does not decrypt encrypted code.'},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
