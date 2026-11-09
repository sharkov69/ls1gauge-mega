#include "Arduino.h"
#include "SdFat.h"
SdFat SD;
char sdUploadName[13]={};
uint32_t sdUploadSyncedSize=0;
bool sdUploadFailed=false,sdUploadFaultLatched=false;
#include "../../ls1gauge-mega/csv_upload_queue.h"
static int failures=0;
#define CHECK(c) do {if(!(c)){printf("FAIL %d: %s\n",__LINE__,#c);++failures;}}while(0)
const uint8_t ns[8]={1,2,3,4,5,6,7,8};
static void add(const char* name,size_t size){disk[name]=std::make_shared<FakeDiskData>();disk[name]->text=std::string(size,'x');}
static void scan(){for(int i=0;i<30&&!csvQueueReady;++i)csvQueueTick();}
int main(){
  uint8_t id[16],next[16],first[16];
  char path[13];
  CHECK(csvQueueSidecar("l-261106.csv",path)&&!strcmp(path,"l-261106.u00"));
  CHECK(csvQueueSidecar("L-261106.BAK",path)&&!strcmp(path,"L-261106.u01"));
  CHECK(csvQueueSidecar("l-261106.b99",path)&&!strcmp(path,"l-261106.u64"));
  CHECK(!csvQueueSidecar("l-261106.b00",path));CHECK(!csvQueueSidecar("settings.csv",path));
  // Interrupt every EEPROM write, both initial allocation and slot rollover.
  for(int seeded=0;seeded<2;++seeded)for(int cut=0;cut<17;++cut){
    EEPROM.reset();fakeEepromWriteBudget=-1;
    if(seeded)CHECK(csvQueueAllocate(first,ns));
    fakeEepromWriteBudget=cut;bool published=false;
    try {published=csvQueueAllocate(id,ns);} catch(const std::runtime_error&){}
    fakeEepromWriteBudget=-1;
    CHECK(csvQueueAllocate(next,ns));
    if(seeded)CHECK(memcmp(first,next,16)!=0);
    if(published)CHECK(memcmp(id,next,16)!=0);
  }
  EEPROM.reset();disk.clear();csvQueueInit(ns);
  CHECK(csvQueueIdentity("l-261106.csv",100,id));memcpy(first,id,16);
  csvQueueInit(ns);CHECK(csvQueueIdentity("l-261106.csv",100,next));CHECK(!memcmp(id,next,16));
  CHECK(csvQueueIdentity("l-261106.csv",200,next));CHECK(!memcmp(id,next,16));
  CHECK(csvQueueIdentity("l-261106.csv",50,next));CHECK(memcmp(id,next,16)!=0);
  CHECK(csvQueueIdentity("l-261106.csv",50,id));CHECK(!memcmp(id,next,16));
  // Torn high-water update preserves the previous valid identity.
  auto meta=disk["l-261106.u00"];
  meta->bytes[24+20]^=1;
  CHECK(csvQueueIdentity("l-261106.csv",50,next));
  meta->readError=true;CHECK(!csvQueueIdentity("l-261106.csv",50,next));meta->readError=false;
  // Existing corrupt initial sidecars fail closed instead of inventing history.
  add("l-261105.u00",0);CHECK(!csvQueueIdentity("l-261105.csv",20,id));
  disk.clear();add("l-261105.bak",70);add("l-261106.csv",100);add("other.csv",100);
  strcpy(sdUploadName,"l-261106.csv");sdUploadSyncedSize=40;
  csvQueueInit(ns);scan();CHECK(csvQueueReady&&!strcmp(csvQueueName,"l-261105.bak"));CHECK(csvQueueLimit()==70);
  csvQueueRelease();scan();CHECK(csvQueueReady&&!strcmp(csvQueueName,"l-261106.csv"));CHECK(csvQueueLimit()==40);
  sdUploadSyncedSize=60;CHECK(csvQueueLimit()==60);memcpy(first,csvQueueId,16);
  csvQueueRelease();scan();CHECK(!csvQueueReady); // Scan cooldown.
  testUs=31000000;scan();CHECK(csvQueueReady);csvQueueRelease();scan();CHECK(csvQueueReady);
  CHECK(!memcmp(first,csvQueueId,16));
  // Logger rotation preserves identity; new daily replacement invalidates it.
  CHECK(SD.rename("l-261106.csv","l-261106.b01"));CHECK(csvQueueRename("l-261106.csv","l-261106.b01"));
  CHECK(!strcmp(csvQueueName,"l-261106.b01"));CHECK(SD.exists("l-261106.u02"));
  CHECK(csvQueueNewFile("l-261106.csv"));add("l-261106.csv",30);
  CHECK(csvQueueIdentity("l-261106.csv",30,next));CHECK(memcmp(first,next,16)!=0);
  sdUploadFaultLatched=true;CHECK(csvQueueLimit()==0);csvQueueRelease();scan();CHECK(!csvQueueReady);
  sdUploadFaultLatched=false;uint8_t zero[8]={};csvQueueInit(zero);scan();CHECK(!csvQueueReady);
  printf("CSV queue failures: %d\n",failures);return failures?1:0;
}
