#include <oni/vcf/ledger.hpp>
#include <fstream>
#include <array>
#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <cerrno>
#endif
namespace oni::vcf {
namespace {
uint32_t crc(std::span<const uint8_t>b){uint32_t x=~0u;for(auto v:b){x^=v;for(int i=0;i<8;++i)x=(x>>1)^(0xedb88320u&uint32_t(-int(x&1)));}return ~x;}
void put(std::vector<uint8_t>&b,uint64_t n,int width){for(int i=0;i<width;i++)b.push_back(uint8_t(n>>(8*i)));}
uint64_t get(std::span<const uint8_t>b,size_t p,size_t n){require(p<=b.size() && n<=b.size()-p,VCF_QUARANTINED);uint64_t v=0;for(size_t i=0;i<n;i++)v|=uint64_t(b[p+i])<<(8*i);return v;}
}
Ledger::Ledger(const std::filesystem::path&p):path_(p){
 require(std::filesystem::is_directory(p.parent_path()));
#ifdef _WIN32
 auto h=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 require(h!=INVALID_HANDLE_VALUE,VCF_CONFLICT);file_=reinterpret_cast<intptr_t>(h);
#else
 auto fd=::open(p.c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0600);require(fd>=0,VCF_INTERNAL);
 if(flock(fd,LOCK_EX|LOCK_NB)!=0){::close(fd);throw Error{VCF_CONFLICT};}file_=fd;
#endif
 try {
  std::ifstream stream(p,std::ios::binary);require(bool(stream),VCF_INTERNAL);
  auto length=std::filesystem::file_size(p);require(length<=64*1024*1024,VCF_CAPACITY);
  std::vector<uint8_t> data(static_cast<size_t>(length));stream.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(length));require(bool(stream)||!length,VCF_INTERNAL);
  size_t pos=0;std::map<uint64_t,uint32_t> last;
  while(pos<data.size()){
   require(get(data,pos,4)==0x31464356,VCF_QUARANTINED);
   auto n=static_cast<size_t>(get(data,pos+4,4));require(n>=20&&n<=1024*1024&&pos+12<=data.size()&&n<=data.size()-pos-12,VCF_QUARANTINED);
   auto body=std::span<const uint8_t>(data).subspan(pos+8,n);require(crc(body)==get(data,pos+8+n,4),VCF_QUARANTINED);
   auto seq=get(body,0,8),tx=get(body,8,8);auto stage=static_cast<uint32_t>(get(body,16,4));
   require(seq==sequence_+1&&tx&&stage>=1&&stage<=6&&last[tx]+1==stage,VCF_QUARANTINED);
   records_.push_back({seq,tx,static_cast<Boundary>(stage),{body.begin()+20,body.end()}});sequence_=seq;last[tx]=stage;pos+=n+12;
  }
#ifdef _WIN32
  LARGE_INTEGER zero{};require(SetFilePointerEx(reinterpret_cast<HANDLE>(file_),zero,nullptr,FILE_END),VCF_INTERNAL);
#else
  require(lseek(static_cast<int>(file_),0,SEEK_END)>=0,VCF_INTERNAL);
  auto dir=::open(p.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);require(dir>=0,VCF_INTERNAL);auto rc=fsync(dir);::close(dir);require(rc==0,VCF_INTERNAL);
#endif
 }catch(...){
#ifdef _WIN32
 CloseHandle(reinterpret_cast<HANDLE>(file_));
#else
 ::close(static_cast<int>(file_));
#endif
 file_=-1;throw;
 }
}
Ledger::~Ledger(){
 if(file_==-1)return;
#ifdef _WIN32
 CloseHandle(reinterpret_cast<HANDLE>(file_));
#else
 ::close(static_cast<int>(file_));
#endif
}
void Ledger::write(std::span<const uint8_t>b){
 size_t pos=0;
 while(pos<b.size()){
#ifdef _WIN32
  DWORD n=0;require(WriteFile(reinterpret_cast<HANDLE>(file_),b.data()+pos,static_cast<DWORD>(b.size()-pos),&n,nullptr)&&n,VCF_INTERNAL);
#else
  auto n=::write(static_cast<int>(file_),b.data()+pos,b.size()-pos);if(n<0&&errno==EINTR)continue;require(n>0,VCF_INTERNAL);
#endif
  pos+=static_cast<size_t>(n);
 }
#ifdef _WIN32
 require(FlushFileBuffers(reinterpret_cast<HANDLE>(file_)),VCF_INTERNAL);
#else
 require(fsync(static_cast<int>(file_))==0,VCF_INTERNAL);
#endif
}
void Ledger::append(uint64_t tx,Boundary boundary,std::span<const uint8_t>payload){
 require(!poisoned_,VCF_QUARANTINED);require(tx&&payload.size()<=1024*1024-20&&sequence_!=UINT64_MAX);
 uint32_t stage=0;for(auto it=records_.rbegin();it!=records_.rend();++it)if(it->transaction==tx){stage=static_cast<uint32_t>(it->boundary);break;}
 require(static_cast<uint32_t>(boundary)==stage+1&&stage<6,VCF_STALE);
 Record record{sequence_+1,tx,boundary,{payload.begin(),payload.end()}};
 std::vector<uint8_t> body;put(body,record.sequence,8);put(body,tx,8);put(body,static_cast<uint32_t>(boundary),4);body.insert(body.end(),payload.begin(),payload.end());
 std::vector<uint8_t> bytes;put(bytes,0x31464356,4);put(bytes,body.size(),4);bytes.insert(bytes.end(),body.begin(),body.end());put(bytes,crc(body),4);
 // Allocate before durability. Once writes begin an error poisons this owner.
 records_.reserve(records_.size()+1);
 try{write(bytes);records_.push_back(std::move(record));++sequence_;}catch(...){poisoned_=true;throw;}
}
std::vector<uint64_t> Ledger::quarantined()const {
 std::set<uint64_t> ids;for(const auto&r:records_)ids.insert(r.transaction);return {ids.begin(),ids.end()};
}
}
