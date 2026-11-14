/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include <stdint.h>
#include <stdbool.h>
struct hercules_pointer_cache {uint64_t x,y;bool down,seen;};
static bool hercules_pointer_update(struct hercules_pointer_cache *s,uint64_t x,uint64_t y,unsigned buttons,uint64_t minx,uint64_t maxx,uint64_t miny,uint64_t maxy,unsigned width,unsigned height){
 if(!s||!width||!height||width>65536||height>65536||maxx<=minx||maxy<=miny||maxx-minx>UINT32_MAX||maxy-miny>UINT32_MAX)return false;
 if(x<minx)x=minx;if(x>maxx)x=maxx;if(y<miny)y=miny;if(y>maxy)y=maxy;
 x=(x-minx)*(width-1)/(maxx-minx);y=(y-miny)*(height-1)/(maxy-miny);
 bool down=(buttons&1)!=0,changed=!s->seen||s->x!=x||s->y!=y||s->down!=down;
 s->x=x;s->y=y;s->down=down;s->seen=true;return changed;
}
