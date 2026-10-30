#pragma once
#include <map>
#include <memory>
#include <vector>
#include <sstream>
#define FILE_WRITE 1
#define O_RDONLY 0
struct FakeDiskData { std::vector<uint8_t> bytes; std::string text; bool error=false; };
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
  int read(){return data&&pos<data->bytes.size()?data->bytes[pos++]:-1;}
  int read(uint8_t* out,size_t n){size_t i=0;for(;i<n;++i){int v=read();if(v<0)break;out[i]=(uint8_t)v;}return (int)i;}
  template<class T> void print(T value){if(data){std::ostringstream s;s<<value;data->text+=s.str();}}
  void print(double value,int places){if(data){char s[40];snprintf(s,sizeof(s),"%.*f",places,value);data->text+=s;}}
  void println(){print('\n');}
  void println(const char* s){print(s);println();}
  bool getWriteError(){return data&&data->error;}
};
struct SdFat {
  bool exists(const char* name){return disk.count(name)!=0;}
  File open(const char* name,int mode=0){
    if(mode&&!exists(name))disk[name]=std::make_shared<FakeDiskData>();
    File f;if(exists(name))f.data=disk[name];return f;
  }
};
