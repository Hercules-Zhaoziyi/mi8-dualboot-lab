#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <setjmp.h>
#include <assert.h>
static jmp_buf env;
static void arrived(void){longjmp(env,1);}
static void put64(void*p,uint64_t v){memcpy(p,&v,8);}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);
 unsigned char*s=mmap(0,8192,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(s!=MAP_FAILED);
 assert(fread(s,1,4096,f)>0);fclose(f);
 unsigned char*fd=mmap((void*)0xce000000,4096,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
 unsigned char*pay=mmap((void*)0xf0000000,4096,3,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);
 assert(fd==(void*)0xce000000&&pay==(void*)0xf0000000);
 put64(s+32,0xce000000);put64(s+40,16);put64(s+48,4096);
 uint32_t tramp[]={0x58000050,0xd61f0200};memcpy(s+4096,tramp,8);put64(s+4104,(uintptr_t)arrived);
 for(int t=0;t<4;t++){
  memset(pay,0xa5,4096);uint32_t magic=t==1?0:0x50554e58,size=t==2?0x04000000:t==3?8:56;
  memcpy(s+4112,&magic,4);memcpy(s+4116,&size,4);memset(s+4120,0x37,56);
  if(!setjmp(env))((void(*)(void))(s+0xa4))();
  if(t==0)assert(!memcmp(pay,s+4112,64));else assert(pay[0]==0xa5);
 }
 puts("PASS ARM64 actual shim: valid XNUP copied, bad magic/oversize/undersize skipped; firmware handoff reached");return 0;
}
