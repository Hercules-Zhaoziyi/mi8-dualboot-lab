/* SPDX-License-Identifier: MIT
 * Bounded FDT /chosen/bootargs charger detector; no hardware or filesystem I/O.
 * Return bits: charger1, valid-header2, bootargs4, other-mode8, malformed32.
 */
#include <stdint.h>
#include <stddef.h>
static uint32_t be32(const unsigned char*p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static int eq(const unsigned char*p,size_t n,const char*s){size_t i=0;while(s[i]){if(i>=n||p[i]!=(unsigned char)s[i])return 0;i++;}return i==n;}
static int ws(unsigned char c){return c==' '||(c>=9&&c<=13);}
static unsigned args(const unsigned char*p,size_t n){
 unsigned flags=4;size_t at=0;
 if(!n||p[n-1])return 32;
 while(at<n&&p[at]){
  while(at<n&&ws(p[at]))at++;
  size_t start=at;while(at<n&&p[at]&&!ws(p[at]))at++;
  size_t len=at-start;
  if(eq(p+start,len,"androidboot.mode=charger")||eq(p+start,len,"androidboot.bootmode=charger"))flags|=1;
  if(eq(p+start,len,"androidboot.mode=normal")||eq(p+start,len,"androidboot.mode=recovery")||eq(p+start,len,"androidboot.mode=bootloader"))flags|=8;
 }
 /* Conflicting modes must not silently divert a normal boot. */
 if((flags&1)&&(flags&8))return 32;
 return flags;
}
unsigned hercules_charger_mode(const void*data,size_t available){
 const unsigned char*b=data;unsigned flags=0,depth=0,chosen=0,seen=0,roots=0;
 if(!b||available<40||be32(b)!=0xd00dfeed)return 0;
 uint32_t total=be32(b+4),os=be32(b+8),ot=be32(b+12),nt=be32(b+32),ns=be32(b+36);
 if(total<40||total>available||total>2*1024*1024||os<40||(os&3)||ot<40||os>total||ns>total-os||ot>total||nt>total-ot||be32(b+20)<17||be32(b+24)>17)return 32;
 const unsigned char*p=b+os,*end=p+ns,*strings=b+ot;flags=2;
 while((size_t)(end-p)>=4){
  unsigned token=be32(p);p+=4;
  if(token==1){
   const unsigned char*name=p;while(p<end&&*p)p++;
   if(p==end)return 34;
   size_t len=p-name;if(!depth&&(len||roots++))return 34;p++;size_t pad=(4-((size_t)(p-b)&3))&3;if((size_t)(end-p)<pad)return 34;p+=pad;
   if(depth==1&&eq(name,len,"chosen")){if(chosen)return 34;chosen=depth+1;}
   if(++depth>64)return 34;
  }else if(token==2){if(!depth)return 34;if(chosen==depth)chosen=0;depth--;
  }else if(token==3){
   if((size_t)(end-p)<8||!depth)return 34;
   uint32_t n=be32(p),nameoff=be32(p+4);p+=8;
   if(n>(size_t)(end-p)||nameoff>=nt)return 34;
   const unsigned char*name=strings+nameoff;size_t length=0;while(length<nt-nameoff&&name[length])length++;
   if(length==nt-nameoff)return 34;
   if(chosen==depth&&eq(name,length,"bootargs")){
    if(seen++)return 34;unsigned f=args(p,n);if(f&32)return 34;flags|=f;
   }
   p+=n;size_t pad=(4-((size_t)(p-b)&3))&3;if((size_t)(end-p)<pad)return 34;p+=pad;
  }else if(token==4){continue;
  }else if(token==9){return depth?34:flags;
  }else return 34;
 }
 return 34;
}
