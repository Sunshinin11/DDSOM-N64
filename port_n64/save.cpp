#include <libdragon.h>
#include "board.h"
#include "save_format.h"
#include "save.h"
namespace ddsom{
namespace{bool present=false;int active=0,step=-1;uint32_t sequence=0,changedAt=0;uint8_t pending[144];}
void SaveInit(cps2recomp::NativeBoard&b){
 active=0;step=-1;sequence=0;changedAt=0;
 present=eeprom_present()!=EEPROM_NONE;if(!present){debugf("[DDSOM_SAVE] EEPROM absent\n");return;}
 uint8_t records[2][144];for(int s=0;s<2;s++)eeprom_read_bytes(records[s],s*144,144);
 bool a=SaveValid(records[0]),c=SaveValid(records[1]);
 if(a||c){active=(!a||(c&&int32_t(Be32(records[1]+4)-Be32(records[0]+4))>0))?1:0;sequence=Be32(records[active]+4);SaveDecode(records[active],b.eeprom.words);}
 debugf("[DDSOM_SAVE] valid=%d slot=%d sequence=%lu\n",a||c,active,(unsigned long)sequence);
}
void SavePoll(cps2recomp::NativeBoard&b){
 if(!present)return;
 if(step<0){if(!b.eeprom.changed){changedAt=0;return;}if(!changedAt){changedAt=get_ticks_us();return;}if(get_ticks_us()-changedAt<1000000)return;
  SaveEncode(pending,b.eeprom.words,sequence+1);b.eeprom.changed=false;step=0;}
 unsigned base=(active^1)*18;uint8_t zero[8]={};
 if(step==0)eeprom_write(base,zero);
 else if(step<=16)eeprom_write(base+1+step,pending+8+step*8); // payload blocks 2..17
 else if(step==17)eeprom_write(base+1,pending+8);
 else {eeprom_write(base,pending);active^=1;sequence++;step=-1;changedAt=0;debugf("[DDSOM_SAVE] committed=%lu\n",(unsigned long)sequence);return;}
 step++;
}
}
