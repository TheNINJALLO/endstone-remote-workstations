#include "item_save.hpp"
#include "inventory_watch.hpp"
#include "../runtime.hpp"
#include "../../native_nbt_output.hpp"
#include <endstone/player.h>
#include <endstone/inventory/item_stack.h>
#include <openssl/evp.h>
#include <dlfcn.h>
#include <link.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <typeinfo>

namespace oni::vcf::platform::linux_native {
namespace {
std::array<uint8_t,32> digest(std::span<const uint8_t> bytes){
 std::array<uint8_t,32> result{};unsigned length=0;
 require(EVP_Digest(bytes.data(),bytes.size(),result.data(),&length,EVP_sha256(),nullptr)==1&&length==result.size(),VCF_INTERNAL);return result;
}
struct Memory {
 struct Region{uintptr_t first,last;bool executable;};std::vector<Region> regions;
 Memory(){
  std::ifstream input("/proc/self/maps");require(bool(input),VCF_UNAVAILABLE);
  for(std::string line;std::getline(input,line);){unsigned long first,last;char mode[5]{};
   if(std::sscanf(line.c_str(),"%lx-%lx %4s",&first,&last,mode)==3&&mode[0]=='r'){
    require(regions.size()<16384,VCF_CAPACITY);regions.push_back({first,last,mode[2]=='x'});
   }
  }
 }
 void readable(uintptr_t p,size_t n,bool executable=false)const{
  if(p&&n<=UINTPTR_MAX-p)for(const auto&r:regions)if(p>=r.first&&p+n<=r.last&&(!executable||r.executable))return;
  throw Error{VCF_UNAVAILABLE};
 }
 template<class T>T field(uintptr_t p,size_t offset=0)const{
  require(offset<=UINTPTR_MAX-p,VCF_UNAVAILABLE);readable(p+offset,sizeof(T));T value;std::memcpy(&value,reinterpret_cast<void*>(p+offset),sizeof(value));return value;
 }
 void function(uintptr_t address,size_t n,std::string_view expected)const{
  readable(address,n,true);auto bytes=digest({reinterpret_cast<const uint8_t*>(address),n});
  constexpr char hex[]="0123456789abcdef";std::array<char,64> actual{};
  for(size_t i=0;i<bytes.size();++i){actual[i*2]=hex[bytes[i]>>4];actual[i*2+1]=hex[bytes[i]&15];}
  require(std::string_view(actual.data(),actual.size())==expected,VCF_UNAVAILABLE);
 }
};
// Linux ELF FDE extents, native callers and independently compiled SDK layouts.
// These are not the legacy Windows held-save addresses or calling convention.
struct Function{uintptr_t rva;size_t size;std::string_view hash;};
constexpr Function functions[]={
 {0xbbb7810,2367,"c144aa9fa63f229e691912937cac78bd9cb505ecf0cec94c8e60d6efa925d781"},
 {0xbcfe480,58,"d01f44d8ced8eb9c28a77dce95eb49e378a3a010022ae9cd9db95c7b264ecc19"},
 {0xbcfe4d0,25,"65513748ca370dd8ed6c201e9da617af53dfb2ff1c14b174e8537de4fb8ee4f9"},
 {0xcc80d10,41,"fdf893165ae75742edbcaba1c541922011f4e2af3e2492cfffd4468ede1237b7"},
 {0xcc80d40,7,"c0aaa941b4b78a3377f57e1761480054a12af422e6930a97444acbea8c2661b7"},
 {0xcc80d50,368,"e5693d2fb42e1f8571f65edc8bf188c9ff0af0c21b5adec9b0967707df98dec4"},
 {0xcc873d0,184,"5358309eb5b0a885e8633af1f8352cf2fb17d4babbf457f706652ae644a7515b"},
 {0xb7af9b0,1319,"02ac6a6a60d3e13bec57fdd4a4a0f6cd534bcada1967cdaef9aff1bc4dd9470c"}
};
struct Bridge {
 uintptr_t bedrock=0,runtime=0;
 Bridge(){
  require(inspect_runtime().accepted,VCF_UNAVAILABLE);
  dl_iterate_phdr([](dl_phdr_info*i,size_t,void*p){if(!i->dlpi_name||!*i->dlpi_name){*static_cast<uintptr_t*>(p)=i->dlpi_addr;return 1;}return 0;},&bedrock);
  Dl_info info{};auto* entry=dlsym(RTLD_DEFAULT,"endstone_get_server");require(entry&&dladdr(entry,&info),VCF_UNAVAILABLE);
  runtime=reinterpret_cast<uintptr_t>(info.dli_fbase);require(bedrock&&runtime,VCF_UNAVAILABLE);
 }
 void verify()const{Memory m;for(const auto&f:functions)m.function(bedrock+f.rva,f.size,f.hash);
  m.function(runtime+0x1d0640,23,"c85a764d6d04dd4e07c1197b8559008e00291cb6a5a6c4b2082a7de7eddfd4b3");
 }
 uintptr_t container(endstone::Player& player)const{
  require(player.isValid()&&!player.isDead(),VCF_CLOSED);auto& inv=player.getInventory();require(inv.getSize()==36,VCF_UNAVAILABLE);
  require(std::string_view(typeid(inv).name())=="N8endstone4core23EndstonePlayerInventoryE",VCF_UNAVAILABLE);
  Memory m;const auto wrapper=reinterpret_cast<uintptr_t>(&inv);const auto table=m.field<uintptr_t>(wrapper);require(table>=16&&wrapper>=8,VCF_UNAVAILABLE);
  require(m.field<intptr_t>(table-16)==-16,VCF_UNAVAILABLE);
  const auto native_player=m.field<uintptr_t>(wrapper,8),native=m.field<uintptr_t>(wrapper-8);m.readable(native,352);
  require(m.field<uintptr_t>(native)==bedrock+0xe64d2e0&&m.field<uintptr_t>(native,344)==native_player,VCF_UNAVAILABLE);
  require(m.field<uintptr_t>(bedrock+0xe64d2e0,8*8)==bedrock+0xbcfe480
   &&m.field<uintptr_t>(bedrock+0xe64d2e0,21*8)==bedrock+0xbcfe4d0,VCF_UNAVAILABLE);
  require(reinterpret_cast<int(*)(const void*)>(bedrock+0xbcfe4d0)(reinterpret_cast<void*>(native))==36,VCF_UNAVAILABLE);return native;
 }
 std::vector<uint8_t> save(uintptr_t native)const{
  uintptr_t tag=0;const uint8_t context=0; // independently verified SaveUseCase::SaveToDisk
  // System V: hidden unique_ptr result first, native ItemStackBase second.
  reinterpret_cast<void(*)(uintptr_t*,const void*,const uint8_t*)>(bedrock+0xbbb7810)(&tag,reinterpret_cast<void*>(native),&context);
  Memory m;require(m.field<uintptr_t>(tag)==bedrock+0xe7af340,VCF_UNAVAILABLE);
  require(m.field<uintptr_t>(bedrock+0xe7af340,8)==bedrock+0xcc80d10
   &&m.field<uintptr_t>(bedrock+0xe7af340,24)==bedrock+0xcc80d40,VCF_UNAVAILABLE);
  // Only this admitted native CompoundTag can reach the native deleting
  // destructor. Linux takes one this argument; no Windows deletion flags.
  struct Owner{uintptr_t tag,destroy;~Owner(){reinterpret_cast<void(*)(void*)>(destroy)(reinterpret_cast<void*>(tag));}} owner{tag,bedrock+0xcc80d10};
  native_nbt::Output output;output.writeByte(10);output.writeString("");
  reinterpret_cast<void(*)(const void*,native_nbt::Interface*)>(bedrock+0xcc80d40)(reinterpret_cast<void*>(tag),&output);
  return output.finish();
 }
 std::vector<uint8_t> slot(uintptr_t container,uint32_t slot)const{
  const auto item=reinterpret_cast<uintptr_t(*)(const void*,int)>(bedrock+0xbcfe480)(reinterpret_cast<void*>(container),static_cast<int>(slot));
  Memory m;m.readable(item,152);require(m.field<uintptr_t>(item)==bedrock+0xe6add70,VCF_UNAVAILABLE);return save(item);
 }
};
}
InventoryItemSnapshot read_inventory_item(endstone::Player& player,uint32_t slot){
 require(slot<36);require(player.isValid()&&!player.isDead(),VCF_CLOSED);
 require(player.hasPermission("remoteworkstations.use")&&player.hasPermission("remoteworkstations.inventory.read"),VCF_DENIED);
 auto watch=watch_inventory(player);const auto stamp=watch.capture();
 static const Bridge bridge;bridge.verify();const auto container=bridge.container(player);
 const auto before=bridge.slot(container,slot);auto item=player.getInventory().getItem(static_cast<int>(slot));require(item.has_value(),VCF_NOT_FOUND);
 const auto identifier=std::string(item->getType().getId());require(!identifier.empty()&&identifier.size()<128,VCF_CAPACITY);
 Memory memory;const auto impl=memory.field<uintptr_t>(reinterpret_cast<uintptr_t>(&*item));memory.readable(impl,136);
 require(memory.field<uintptr_t>(impl)==bridge.runtime+0xabf5b0,VCF_UNAVAILABLE);
 require(item->getAmount()>0&&item->getAmount()<=255&&memory.field<uint8_t>(impl+8,34)==item->getAmount(),VCF_UNAVAILABLE);
 auto bytes=bridge.save(impl+8);require(bytes==before&&bytes==bridge.slot(container,slot),VCF_STALE);
 require(bridge.container(player)==container,VCF_STALE);
 require(player.hasPermission("remoteworkstations.use")&&player.hasPermission("remoteworkstations.inventory.read"),VCF_DENIED);
 watch.validate(player,stamp);
 InventoryItemSnapshot result;result.info.size=sizeof(result.info);result.info.version=VCF_ABI_VERSION;
 result.info.slot=slot;result.info.amount=static_cast<uint32_t>(item->getAmount());result.info.auxiliary=item->getData();
 result.info.nbt_format=VCF_BDS_ITEM_SAVE_NBT;result.info.nbt_bytes=static_cast<uint32_t>(bytes.size());
 std::memcpy(result.info.identifier,identifier.c_str(),identifier.size()+1);const auto hash=digest(bytes);std::copy(hash.begin(),hash.end(),result.info.digest);
 result.nbt=std::move(bytes);return result;
}
std::function<void()> observe_inventory_item(endstone::Player& player,uint32_t slot,std::function<endstone::Player*()> resolve){
 require(slot<36&&bool(resolve));auto watch=watch_inventory(player);const auto stamp=watch.capture();
 auto initial=read_inventory_item(player,slot);watch.validate(player,stamp);
 return [watch,stamp,initial=std::move(initial),slot,resolve=std::move(resolve)]{
  auto* current=resolve();require(current,VCF_CLOSED);watch.validate(*current,stamp);
  auto saved=read_inventory_item(*current,slot);
  require(saved.nbt==initial.nbt,VCF_STALE);watch.validate(*current,stamp);
 };
}
}
