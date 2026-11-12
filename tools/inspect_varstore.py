#!/usr/bin/env python3
"""Read-only offline check of a PRIVATE RDWVAR archive; never writes a device."""
from pathlib import Path
import argparse,mmap,struct,zlib,json
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('archive',type=Path);a=p.parse_args()
 with a.archive.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
  magic,version,slot,generation,n,count,seconds,crc,hcrc=struct.unpack_from('<8sIIQQQQII',data);h=bytearray(data[:56]);struct.pack_into('<I',h,52,0)
  assert magic==b'RDWVAR\0\0' and zlib.crc32(h)==hcrc and 4096+n<=len(data)
  body=memoryview(data)[4096:4096+n];assert zlib.crc32(body)==crc;pos=0
  for i in range(count):
   kind,mode,uid,gid,flags,plen,nx,ns,sec,size=struct.unpack_from('<8IqQ',body,pos);pos+=48+plen
   for j in range(nx):nl,vl=struct.unpack_from('<II',body,pos);pos+=8+nl+vl
   if kind in (2,3,4):pos+=size
   pos=(pos+7)&~7;assert pos<=n
  assert pos==n;body.release();print(json.dumps({'version':version,'slot':slot,'generation':generation,'bytes':n,'entries':count,'header_crc':True,'body_crc':True,'record_boundaries':True},indent=2))
if __name__=='__main__':main()
