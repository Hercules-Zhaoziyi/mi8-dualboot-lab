#!/usr/bin/env python3
"""Mi8 legacy-boot composer: local licensed inputs only; never writes a phone."""
from pathlib import Path
import argparse,hashlib,struct,zlib,gzip,subprocess,json
p=argparse.ArgumentParser(description=__doc__)
for n in ['original-boot','firmware','shim','kernelpatcher','payload','out']:p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--original-sha256',required=True);p.add_argument('--boot-partition-bytes',type=int,required=True);p.add_argument('--device',choices=['dipper'],required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];out=a.out.resolve()
if not out.is_relative_to((repo/'local').resolve()) or out.exists():raise SystemExit('Use a new output directory under local/')
orig=a.original_boot.read_bytes();assert hashlib.sha256(orig).hexdigest()==a.original_sha256.lower();assert orig[:8]==b'ANDROID!' and len(orig)>=4096
page=struct.unpack_from('<I',orig,36)[0];ks=struct.unpack_from('<I',orig,8)[0];rs=struct.unpack_from('<I',orig,16)[0]
assert page==4096 and struct.unpack_from('<I',orig,24)[0]==0 and struct.unpack_from('<Q',orig,1632)[0]==0
assert struct.unpack_from('<I',orig,40)[0]==0,'Only legacy header version zero is supported'
ramstart=page+((ks+page-1)//page)*page;assert ramstart+rs<=len(orig)
dec=zlib.decompressobj(31);linux=dec.decompress(orig[page:page+ks]);assert dec.eof;dtb=dec.unused_data;assert dtb[:4]==bytes.fromhex('d00dfeed')
assert linux[56:60]==b'ARM\x64';fd=a.firmware.read_bytes();assert len(fd)==0x700000
raw=a.payload.read_bytes();magic,total=struct.unpack_from('<II',raw);assert magic==0x50554e58 and total==len(raw)-8
pos=8;body=bytearray();ended=False
while pos+24<=len(raw):
 name,n,res=struct.unpack_from('<16sII',raw,pos);start=pos;pos+=24
 if name[0]==0:ended=True;break
 assert pos+n+(-n%16)<=len(raw)
 pos+=n+(-n%16)
 if name.rstrip(b'\0')!=b'usb':body+=raw[start:pos]
assert ended
body+=bytes(24);payload=struct.pack('<II',magic,len(body))+body
assert b'loader_return_after=' not in payload,'Use a daily payload without runtime return limit'
out.mkdir(parents=True);patchedfile=out/'android-kernel-patched.bin';patchedfile.write_bytes(linux)
subprocess.run([str(a.kernelpatcher.resolve()),str(patchedfile),str(a.shim.resolve()),str(len(fd)+len(payload))],check=True)
patched=bytearray(patchedfile.read_bytes());shim=a.shim.read_bytes();assert len(patched)==len(linux) and patched[len(shim):]==linux[len(shim):]
old=struct.unpack_from('<I',linux)[0];new=struct.unpack_from('<I',patched,4)[0];assert old&0xfc000000==0x14000000 and new&0xfc000000==0x14000000
assert (old&0x03ffffff)*4==4+(new&0x03ffffff)*4
struct.pack_into('<Q',patched,16,len(patched)+len(fd)+len(payload));assert struct.unpack_from('<Q',patched,48)[0]==len(linux)
uncompressed=bytes(patched)+fd+payload
compressed=subprocess.run(['libdeflate-gzip','-12','-c'],input=uncompressed,capture_output=True,check=True).stdout;assert gzip.decompress(compressed)==uncompressed
kernel=compressed+dtb;ram=orig[ramstart:ramstart+rs];header=bytearray(orig[:page]);struct.pack_into('<I',header,8,len(kernel))
digest=hashlib.sha1(kernel+struct.pack('<I',len(kernel))+ram+struct.pack('<III',rs,0,0)).digest();header[576:608]=digest+bytes(12)
image=header+kernel+bytes(-len(kernel)%page)+ram+bytes(-len(ram)%page)
assert 0<len(image)<=a.boot_partition_bytes,'Candidate exceeds measured partition capacity'
dest=out/'boot-local-candidate.img';dest.write_bytes(image)
report={'device':'dipper','inputs':{n:hashlib.sha256(getattr(a,n).read_bytes()).hexdigest() for n in ['original_boot','firmware','shim','kernelpatcher','payload']},'candidate_sha256':hashlib.sha256(image).hexdigest(),'bytes':len(image),'original_ramdisk_sha256':hashlib.sha256(ram).hexdigest(),'original_dtb_sha256':hashlib.sha256(dtb).hexdigest(),'reserved_decompressed_bytes':len(uncompressed),'device_test':'NOT RUN','flashed':False}
(out/'composition-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
