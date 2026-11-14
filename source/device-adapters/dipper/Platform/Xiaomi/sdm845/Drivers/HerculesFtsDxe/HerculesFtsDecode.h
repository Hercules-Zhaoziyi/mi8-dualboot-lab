/* SPDX-License-Identifier: MIT */
#include <stdint.h>
struct hfts_point {unsigned x,y,id,down;};
static int hfts_decode(struct hfts_point*s,const uint8_t e[8]){
 unsigned id=e[1]>>4,type=e[1]&15,x=((unsigned)e[3]&15)*256+e[2],y=(unsigned)e[4]*16+(e[3]>>4);
 if(e[0]==0)return 0;
 if(e[0]!=0x13&&e[0]!=0x23&&e[0]!=0x33)return 0;
 if(id>=10)return 0;
 if(e[0]==0x33||type==5){if(s->down&&s->id==id){s->down=0;return 1;}return 0;}
 if(type!=1&&type!=2&&type!=3)return 0;
 if(x>1079||y>2247)return 0;
 if(s->down&&s->id!=id)return 0;
 if(!s->down&&e[0]!=0x13)return 0;
 int changed=!s->down||s->x!=x||s->y!=y;s->down=1;s->id=id;s->x=x;s->y=y;return changed;
}
