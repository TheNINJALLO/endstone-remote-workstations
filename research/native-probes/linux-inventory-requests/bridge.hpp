#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <endstone/inventory/item_stack.h>
#include <endstone/nbt/tag.h>
#include "framework/held_items.hpp"
#include "framework/platform/runtime.hpp"
#include <openssl/evp.h>
#include <dlfcn.h>
#include <link.h>
#include <fstream>
#include <cstdio>
#include <bit>
#include <nlohmann/json.hpp>
using namespace oni::vcf;
using nlohmann::json;
std::string hash(std::span<const uint8_t> bytes){
 std::array<unsigned char,32> d{};unsigned n=0;require(EVP_Digest(bytes.data(),bytes.size(),d.data(),&n,EVP_sha256(),nullptr)==1&&n==32);
 constexpr char hex[]="0123456789abcdef";std::string out;for(auto c:d){out+=hex[c>>4];out+=hex[c&15];}return out;
}
struct Memory {
 struct R{uintptr_t first,last;bool executable;};std::vector<R> regions;
 Memory(){std::ifstream input("/proc/self/maps");for(std::string line;std::getline(input,line);){unsigned long a,b;char p[5]{};if(std::sscanf(line.c_str(),"%lx-%lx %4s",&a,&b,p)==3&&p[0]=='r')regions.push_back({a,b,p[2]=='x'});}}
 void check(uintptr_t p,size_t n,bool x=false)const{if(p&&n<=UINTPTR_MAX-p)for(auto&r:regions)if(p>=r.first&&p+n<=r.last&&(!x||r.executable))return;throw Error{VCF_UNAVAILABLE};}
 template<class T>T get(uintptr_t p,size_t off=0)const{require(off<=UINTPTR_MAX-p);check(p+off,sizeof(T));T v;std::memcpy(&v,reinterpret_cast<void*>(p+off),sizeof(v));return v;}
};
// Pinned Linux IDataOutput ABI: two destructors followed by these methods.
// This writes little-endian NBT, preserving native list types and bit patterns.
struct NativeOutput {
 virtual ~NativeOutput()=default;
 virtual void writeString(std::string_view v)=0;
 virtual void writeLongString(std::string_view v)=0;
 virtual void writeFloat(float)=0;
 virtual void writeDouble(double)=0;
 virtual void writeByte(char)=0;
 virtual void writeShort(int16_t)=0;
 virtual void writeInt(int32_t)=0;
 virtual void writeLongLong(int64_t)=0;
 virtual void writeBytes(const void*,size_t)=0;
};
struct Output final:NativeOutput {
 std::vector<uint8_t> bytes;size_t calls=0;
 void room(size_t n){require(++calls<16384&&n<=65536&&bytes.size()<=65536-n,VCF_CAPACITY);}
 void put(uint64_t n,unsigned width){room(width);for(unsigned i=0;i<width;++i)bytes.push_back(static_cast<uint8_t>(n>>(8*i)));}
 void writeString(std::string_view v)override{require(v.size()<=UINT16_MAX);put(v.size(),2);writeBytes(v.data(),v.size());}
 void writeLongString(std::string_view v)override{require(v.size()<=65536);put(v.size(),4);writeBytes(v.data(),v.size());}
 void writeFloat(float v)override{put(std::bit_cast<uint32_t>(v),4);}
 void writeDouble(double v)override{put(std::bit_cast<uint64_t>(v),8);}
 void writeByte(char v)override{put(static_cast<uint8_t>(v),1);}
 void writeShort(int16_t v)override{put(static_cast<uint16_t>(v),2);}
 void writeInt(int32_t v)override{put(static_cast<uint32_t>(v),4);}
 void writeLongLong(int64_t v)override{put(static_cast<uint64_t>(v),8);}
 void writeBytes(const void* p,size_t n)override{room(n);if(n){require(p);const auto*b=static_cast<const uint8_t*>(p);bytes.insert(bytes.end(),b,b+n);}}
};
struct Bridge {
 uintptr_t bds=0,runtime=0;json fingerprints;
 Bridge(const std::filesystem::path& path){
  require(platform::inspect_runtime().accepted,VCF_UNAVAILABLE);
  dl_iterate_phdr([](dl_phdr_info*i,size_t,void*p){if(!i->dlpi_name||!*i->dlpi_name){*static_cast<uintptr_t*>(p)=i->dlpi_addr;return 1;}return 0;},&bds);
  Dl_info i{};require(dladdr(dlsym(RTLD_DEFAULT,"endstone_get_server"),&i));runtime=reinterpret_cast<uintptr_t>(i.dli_fbase);
  std::ifstream input(path);input>>fingerprints;
 }
 uintptr_t fn(uintptr_t rva,bool rt=false)const{
  for(const auto&f:fingerprints)if(f.at("rva").get<uintptr_t>()==rva&&f.value("runtime",false)==rt){auto address=(rt?runtime:bds)+rva;Memory{}.check(address,f.at("size").get<size_t>(),true);require(hash({reinterpret_cast<const uint8_t*>(address),f.at("size").get<size_t>()})==f.at("sha256").get<std::string>(),VCF_UNAVAILABLE);return address;}
  throw Error{VCF_NOT_FOUND};
 }
 struct Tag {
  uintptr_t p=0,destroy=0;
  Tag(uintptr_t v,uintptr_t d):p(v),destroy(d){}
  Tag(const Tag&)=delete;Tag(Tag&&o)noexcept:p(o.p),destroy(o.destroy){o.p=0;}
  ~Tag(){if(p)reinterpret_cast<void(*)(void*)>(destroy)(reinterpret_cast<void*>(p));}
 };
 Tag owned(uintptr_t p,bool list)const{
  auto destroy=fn(list?0xcc8f710:0xcc80d10);Memory m;
  require(m.get<uintptr_t>(p)==bds+(list?0xe7af5e0:0xe7af340),VCF_UNAVAILABLE);
  require(m.get<uintptr_t>(m.get<uintptr_t>(p),8)==destroy,VCF_UNAVAILABLE);return Tag(p,destroy);
 }
 Tag save(uintptr_t native,bool inventory=false)const{
  uintptr_t out=0;uint8_t context=0;
  auto save=fn(inventory?0xbcfdea0:0xbbb7810);
  reinterpret_cast<void(*)(uintptr_t*,const void*,const uint8_t*)>(save)(&out,reinterpret_cast<void*>(native),&context);
  return owned(out,inventory);
 }
 std::vector<uint8_t> bytes(const Tag& tag,bool list=false)const{
  auto write=fn(list?0xcc873c0:0xcc80d40);Memory m;require(m.get<uintptr_t>(m.get<uintptr_t>(tag.p),24)==write);
  Output out;out.writeByte(10);out.writeString("");if(list){out.writeByte(9);out.writeString("Inventory");}
  reinterpret_cast<void(*)(const void*,NativeOutput*)>(write)(reinterpret_cast<void*>(tag.p),&out);
  if(list)out.writeByte(0);validate_nbt(out.bytes);return out.bytes;
 }
 endstone::nbt::Tag convert(const Tag& tag)const{return reinterpret_cast<endstone::nbt::Tag(*)(const void*)>(fn(0x213fd0,true))(reinterpret_cast<void*>(tag.p));}
 uintptr_t container(endstone::Player& player)const{
  auto& inv=player.getInventory();require(inv.getSize()==36);require(std::string_view(typeid(inv).name())=="N8endstone4core23EndstonePlayerInventoryE");
  Memory m;const auto wrapper=reinterpret_cast<uintptr_t>(&inv);const auto vt=m.get<uintptr_t>(wrapper);require(m.get<intptr_t>(vt-16)==-16);
  const auto native_player=m.get<uintptr_t>(wrapper,8);const auto p=m.get<uintptr_t>(wrapper-8);m.check(p,352);
  require(m.get<uintptr_t>(p)==bds+0xe64d2e0&&m.get<uintptr_t>(p,344)==native_player);
  require(m.get<uintptr_t>(bds+0xe64d2e0,51*8)==fn(0xbcfdea0));
  require(m.get<uintptr_t>(bds+0xe64d2e0,8*8)==fn(0xbcfe480));
  require(m.get<uintptr_t>(bds+0xe64d2e0,21*8)==fn(0xbcfe4d0));
  require(reinterpret_cast<int(*)(const void*)>(fn(0xbcfe4d0))(reinterpret_cast<void*>(p))==36);return p;
 }
};
