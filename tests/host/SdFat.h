#pragma once
#include <map>
#include <memory>
#include <vector>
#include <sstream>
#define FILE_WRITE 1
#define O_RDONLY 0
inline uint16_t FS_DATE(int y,int m,int d){return (uint16_t)(((y-1980)<<9)|(m<<5)|d);}
inline uint16_t FS_TIME(int h,int m,int s){return (uint16_t)((h<<11)|(m<<5)|(s/2));}
namespace FsDateTime {
inline void (*callback)(uint16_t*,uint16_t*)=nullptr;
inline void setCallback(void (*fn)(uint16_t*,uint16_t*)){callback=fn;}
}
struct FakeDiskData { std::vector<uint8_t> bytes; std::string text; bool error=false,readError=false; unsigned syncs=0; uint16_t date=0,time=0; };
inline std::map<std::string,std::shared_ptr<FakeDiskData>> disk;
struct File {
  std::shared_ptr<FakeDiskData> data;
  size_t pos=0;
  operator bool() const {return !!data;}
  void close(){data.reset();}
  bool open(const char*){return false;}
  bool openNext(File*,int){return false;}
  bool isDir(){return false;}
  void getName(char* p,size_t){*p=0;}
  bool seek(uint32_t p){pos=p;return !!data;}
  int read(){if(!data||data->readError)return -1;if(!data->bytes.empty())return pos<data->bytes.size()?data->bytes[pos++]:-1;return pos<data->text.size()?(uint8_t)data->text[pos++]:-1;}
  uint8_t getError(){return data&&(data->error||data->readError)?1:0;}
  uint32_t size(){return data?(uint32_t)(data->text.size()+data->bytes.size()):0;}
  int read(uint8_t* out,size_t n){size_t i=0;for(;i<n;++i){int v=read();if(v<0)break;out[i]=(uint8_t)v;}return (int)i;}
  template<class T> void print(T value){if(data){std::ostringstream s;s<<value;data->text+=s.str();}}
  void print(double value,int places){if(data){char s[40];snprintf(s,sizeof(s),"%.*f",places,value);data->text+=s;}}
  void println(){print('\n');}
  void println(const char* s){print(s);println();}
  bool getWriteError(){return data&&data->error;}
  bool sync(){if(!data)return false;++data->syncs;if(FsDateTime::callback)FsDateTime::callback(&data->date,&data->time);return !data->error;}
};
struct SdFat {
  bool rename(const char* oldName,const char* newName){if(!exists(oldName)||exists(newName))return false;disk[newName]=disk[oldName];disk.erase(oldName);return true;}
  bool exists(const char* name){return disk.count(name)!=0;}
  File open(const char* name,int mode=0){
    if(mode&&!exists(name)){disk[name]=std::make_shared<FakeDiskData>();if(FsDateTime::callback)FsDateTime::callback(&disk[name]->date,&disk[name]->time);}
    File f;if(exists(name))f.data=disk[name];return f;
  }
};
