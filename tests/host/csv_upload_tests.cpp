#include "Arduino.h"
#include <vector>
#include "../../ls1gauge-mega/csv_upload.h"
#include "../../ls1gauge-mega/csv_upload_sd.h"
static int failures=0;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);++failures;}}while(0)
static uint8_t token[32], id[16];
static std::vector<uint8_t> source(700);
static int reads=0;
static int readBytes(void*,uint32_t offset,uint8_t* dest,uint16_t length){
  ++reads;if(offset+length>source.size())return -1;
  memcpy(dest,source.data()+offset,length);return length;
}
static std::vector<uint8_t> ack(uint32_t offset){
  std::vector<uint8_t> b={'P','5','9','A'};b.insert(b.end(),id,id+16);
  for(int i=0;i<4;++i)b.push_back((uint8_t)(offset>>(8*i)));return b;
}
int main(){
  for(int i=0;i<32;++i)token[i]=(uint8_t)(i+1);
  for(int i=0;i<16;++i)id[i]=(uint8_t)(0xa0+i);
  for(size_t i=0;i<source.size();++i)source[i]=(uint8_t)i;
  CsvUpload upload;
  CHECK(sizeof(upload)<=1120);
  CHECK(upload.prepare(true,700,readBytes,nullptr)==0); // Provision first.
  CHECK(upload.begin(token,id));
  CHECK(upload.prepare(false,700,readBytes,nullptr)==0);CHECK(reads==0);
  CHECK(upload.prepare(true,700,readBytes,nullptr)==58);
  CHECK(memcmp(upload.data(),"P59C",4)==0);
  CHECK(memcmp(upload.data()+4,token,32)==0&&memcmp(upload.data()+36,id,16)==0);
  CHECK(memcmp(upload.data()+52,"\xff\xff\xff\xff\0\0",6)==0);
  CHECK(upload.prepare(true,700,readBytes,nullptr)==0); // One immutable in-flight request.
  auto response=ack(100);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.offset()==100);
  CHECK(upload.prepare(true,700,readBytes,nullptr)==658);
  CHECK(memcmp(upload.data()+52,"\x64\0\0\0\x58\x02",6)==0);
  CHECK(memcmp(upload.data()+58,source.data()+100,600)==0);
  response=ack(356);CHECK(!upload.acknowledge(false,200,response.data(),24));
  CHECK(upload.offset()==100); // An unauthenticated ACK never advances progress.
  CHECK(upload.prepare(true,700,readBytes,nullptr)==58); // Resolve uncertain POST with status.
  CHECK(upload.acknowledge(true,200,response.data(),24));CHECK(upload.offset()==356);
  CHECK(upload.prepare(true,500,readBytes,nullptr)==202); // Never read unsynced tail.
  response=ack(500);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,500,readBytes,nullptr)==0);CHECK(upload.offset()==500);
  CHECK(upload.prepare(true,700,readBytes,nullptr)==258); // Growing same-day file.
  upload.retry(); // Lost ACK; pretend server committed before link died.
  CHECK(upload.prepare(true,700,readBytes,nullptr)==58);
  response=ack(700);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,700,readBytes,nullptr)==0);
  CsvUpload restarted;CHECK(restarted.begin(token,id));
  CHECK(restarted.prepare(true,700,readBytes,nullptr)==58);
  CHECK(restarted.acknowledge(true,200,response.data(),24));CHECK(restarted.offset()==700);
  // Wrong ID, malformed ACK, HTTP error, overflow and remote-ahead must not advance.
  for(int kind=0;kind<7;++kind){
    CHECK(upload.begin(token,id));CHECK(upload.prepare(true,700,readBytes,nullptr)==58);
    response=ack(kind==4?701:kind==5?0xffffffffu:100);
    if(kind==0)response[4]^=1;
    if(kind==1)response[0]='X';
    if(kind==6)response.push_back(0);
    CHECK(!upload.acknowledge(true,kind==3?409:200,response.data(),kind==2?23:kind==6?25:24));
    CHECK(upload.offset()==0);
  }
  CHECK(upload.begin(token,id));CHECK(upload.prepare(true,700,readBytes,nullptr)==58);
  response=ack(0);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,700,readBytes,nullptr)==758);
  response=ack(255);CHECK(!upload.acknowledge(true,200,response.data(),24));CHECK(upload.offset()==0);
  // A short SD read cannot become a valid request containing stale buffer bytes.
  CHECK(upload.prepare(true,1000,readBytes,nullptr)==58);response=ack(700);
  CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,1000,readBytes,nullptr)==0);CHECK(upload.offset()==700);
  // SdFat adapter reads existing files only and leaves the writer untouched.
  SdFat sd;CsvUploadSdSource diskSource(sd,"l-261106.csv");
  auto file=std::make_shared<FakeDiskData>();file->bytes=source;disk["l-261106.csv"]=file;
  uint8_t bytes[20];CHECK(CsvUploadSdSource::read(&diskSource,690,bytes,10)==10);
  CHECK(memcmp(bytes,source.data()+690,10)==0);
  CHECK(CsvUploadSdSource::read(&diskSource,690,bytes,20)<0);
  file->readError=true;CHECK(CsvUploadSdSource::read(&diskSource,0,bytes,10)<0);
  CsvUploadSdSource missing(sd,"missing.csv");CHECK(CsvUploadSdSource::read(&missing,0,bytes,10)<0);
  CHECK(disk.count("missing.csv")==0);
  // One full 1024-byte payload followed by the exact remaining bytes.
  source.resize(1800);
  for(size_t i=0;i<source.size();++i)source[i]=(uint8_t)i;
  CHECK(upload.begin(token,id));CHECK(upload.prepare(true,1800,readBytes,nullptr)==58);
  response=ack(0);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,1800,readBytes,nullptr)==1082);
  CHECK(memcmp(upload.data()+52,"\0\0\0\0\0\x04",6)==0);
  CHECK(memcmp(upload.data()+58,source.data(),1024)==0);
  response=ack(1024);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,1800,readBytes,nullptr)==834);
  CHECK(memcmp(upload.data()+52,"\0\x04\0\0\x08\x03",6)==0);
  CHECK(memcmp(upload.data()+58,source.data()+1024,776)==0);
  response=ack(1800);CHECK(upload.acknowledge(true,200,response.data(),24));
  CHECK(upload.prepare(true,1800,readBytes,nullptr)==0);CHECK(upload.status()==CsvUpload::AT_END);
  uint8_t zero[32]={};CHECK(!upload.begin(zero,id));CHECK(!upload.begin(token,zero));
  CHECK(upload.prepare(true,700,readBytes,nullptr)==0);
  printf("CSV upload failures: %d; object bytes: %zu\n",failures,sizeof(upload));return failures?1:0;
}
